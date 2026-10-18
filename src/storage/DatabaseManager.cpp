#include "DatabaseManager.hpp"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>

#include <QThread>
#include <QFileInfo>

DatabaseManager& DatabaseManager::instance() {
    static DatabaseManager inst;
    return inst;
}

QSqlDatabase DatabaseManager::get_database() {
    QString conn_name = QString("conn_%1").arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
    if (QSqlDatabase::contains(conn_name)) {
        return QSqlDatabase::database(conn_name);
    }
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", conn_name);
    db.setDatabaseName(db_path_);
    if (db.open()) {
        QSqlQuery q(db);
        q.exec("PRAGMA journal_mode = WAL;");
        q.exec("PRAGMA synchronous = NORMAL;");
    }
    return db;
}

bool DatabaseManager::init_database(const QString& db_path) {
    db_path_ = db_path;
    QFileInfo fi(db_path);
    QDir dir = fi.dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QSqlDatabase db = get_database();
    if (!db.isOpen()) {
        qCritical() << "[DatabaseManager] Failed to open SQLite:" << db.lastError().text();
        return false;
    }

    QSqlQuery q(db);
    // 强制开启 WAL 模式以支撑多线程高频无锁读写
    q.exec("PRAGMA journal_mode = WAL;");
    q.exec("PRAGMA synchronous = NORMAL;");

    // 1. ROI 配置历史版本表
    q.exec(
        "CREATE TABLE IF NOT EXISTS roi_configs ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  source_id TEXT NOT NULL,"
        "  roi_version INTEGER NOT NULL,"
        "  ref_width INTEGER NOT NULL,"
        "  ref_height INTEGER NOT NULL,"
        "  vertices_json TEXT NOT NULL,"
        "  buffer_distance REAL DEFAULT 20.0,"
        "  min_confirm_frames INTEGER DEFAULT 4,"
        "  invert_direction INTEGER DEFAULT 0,"
        "  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ");"
    );
    q.exec("ALTER TABLE roi_configs ADD COLUMN min_confirm_frames INTEGER DEFAULT 4;");
    q.exec("ALTER TABLE roi_configs ADD COLUMN invert_direction INTEGER DEFAULT 0;");

    // 2. 通行明细表 (含短视频切片与属性)
    q.exec(
        "CREATE TABLE IF NOT EXISTS passage_events ("
        "  event_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  source_id TEXT NOT NULL,"
        "  timestamp_ms INTEGER NOT NULL,"
        "  track_id INTEGER NOT NULL,"
        "  direction INTEGER NOT NULL,"
        "  roi_version INTEGER NOT NULL,"
        "  confidence REAL NOT NULL,"
        "  clip_path TEXT DEFAULT '',"
        "  gender TEXT DEFAULT '未知',"
        "  age_group TEXT DEFAULT '未知'"
        ");"
    );
    q.exec("CREATE INDEX IF NOT EXISTS idx_events_ts ON passage_events(timestamp_ms);");

    return true;
}

bool DatabaseManager::insert_events(const std::vector<PassageEvent>& events) {
    if (events.empty()) return true;

    QSqlDatabase db = get_database();
    if (!db.isOpen()) return false;
    db.transaction();

    QSqlQuery q(db);
    q.prepare("INSERT INTO passage_events ("
              "source_id, timestamp_ms, track_id, direction, roi_version, confidence, clip_path, gender, age_group) "
              "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");

    for (const auto& ev : events) {
        q.addBindValue(QString::fromStdString(ev.source_id));
        q.addBindValue(static_cast<qint64>(ev.timestamp_ms));
        q.addBindValue(ev.track_id);
        q.addBindValue(static_cast<int>(ev.direction));
        q.addBindValue(ev.roi_version);
        q.addBindValue(ev.confidence);
        q.addBindValue(QString::fromStdString(ev.clip_path));
        q.addBindValue(QString::fromStdString(ev.gender));
        q.addBindValue(QString::fromStdString(ev.age_group));
        q.exec();
    }

    return db.commit();
}

std::vector<PassageEvent> DatabaseManager::query_filtered_events(int direction_filter, int64_t start_ms, int64_t end_ms, const QString& keyword, int limit) {
    std::vector<PassageEvent> list;
    QString sql = "SELECT timestamp_ms, track_id, direction, roi_version, confidence, source_id, clip_path, gender, age_group "
                  "FROM passage_events WHERE 1=1";

    if (direction_filter >= 0) {
        sql += " AND direction = ?";
    }
    if (start_ms > 0) {
        sql += " AND timestamp_ms >= ?";
    }
    if (end_ms > 0) {
        sql += " AND timestamp_ms <= ?";
    }
    if (!keyword.trimmed().isEmpty()) {
        sql += " AND (CAST(track_id AS TEXT) LIKE ? OR clip_path LIKE ? OR gender LIKE ? OR age_group LIKE ?)";
    }
    sql += " ORDER BY timestamp_ms DESC LIMIT ?";

    QSqlDatabase db = get_database();
    QSqlQuery q(db);
    q.prepare(sql);
    if (direction_filter >= 0) {
        q.addBindValue(direction_filter);
    }
    if (start_ms > 0) {
        q.addBindValue(static_cast<qint64>(start_ms));
    }
    if (end_ms > 0) {
        q.addBindValue(static_cast<qint64>(end_ms));
    }
    if (!keyword.trimmed().isEmpty()) {
        QString kw = "%" + keyword.trimmed() + "%";
        q.addBindValue(kw);
        q.addBindValue(kw);
        q.addBindValue(kw);
        q.addBindValue(kw);
    }
    q.addBindValue(limit);

    if (q.exec()) {
        while (q.next()) {
            PassageEvent ev;
            ev.timestamp_ms = q.value(0).toLongLong();
            ev.track_id = q.value(1).toInt();
            ev.direction = static_cast<PassageDirection>(q.value(2).toInt());
            ev.roi_version = q.value(3).toInt();
            ev.confidence = q.value(4).toFloat();
            ev.source_id = q.value(5).toString().toStdString();
            ev.clip_path = q.value(6).toString().toStdString();
            ev.gender = q.value(7).toString().toStdString();
            ev.age_group = q.value(8).toString().toStdString();
            list.push_back(ev);
        }
    }
    return list;
}

std::vector<PassageEvent> DatabaseManager::query_recent_events(int limit) {
    return query_filtered_events(-1, 0, 0, "", limit);
}

std::vector<PassageEvent> DatabaseManager::query_events_by_timerange(int64_t start_ms, int64_t end_ms, int limit) {
    std::vector<PassageEvent> list;
    QSqlDatabase db = get_database();
    QSqlQuery q(db);
    q.prepare("SELECT timestamp_ms, track_id, direction, roi_version, confidence, source_id, clip_path, gender, age_group "
              "FROM passage_events WHERE timestamp_ms >= ? AND timestamp_ms <= ? "
              "ORDER BY timestamp_ms ASC LIMIT ?");
    q.addBindValue(static_cast<qint64>(start_ms));
    q.addBindValue(static_cast<qint64>(end_ms));
    q.addBindValue(limit);

    if (q.exec()) {
        while (q.next()) {
            PassageEvent ev;
            ev.timestamp_ms = q.value(0).toLongLong();
            ev.track_id = q.value(1).toInt();
            ev.direction = static_cast<PassageDirection>(q.value(2).toInt());
            ev.roi_version = q.value(3).toInt();
            ev.confidence = q.value(4).toFloat();
            ev.source_id = q.value(5).toString().toStdString();
            ev.clip_path = q.value(6).toString().toStdString();
            ev.gender = q.value(7).toString().toStdString();
            ev.age_group = q.value(8).toString().toStdString();
            list.push_back(ev);
        }
    }
    return list;
}

bool DatabaseManager::save_roi_config(const RoiConfig& config) {
    if (config.vertices.size() < 3) return false;

    QJsonArray arr;
    for (const auto& pt : config.vertices) {
        QJsonObject obj;
        obj["x"] = pt.x;
        obj["y"] = pt.y;
        arr.append(obj);
    }
    QJsonDocument doc(arr);
    QString json_str = doc.toJson(QJsonDocument::Compact);

    std::string src_id = config.source_id.empty() ? "default" : config.source_id;
    int ver = config.version > 0 ? config.version : 1;

    QSqlDatabase db = get_database();
    QSqlQuery q(db);
    q.prepare("INSERT INTO roi_configs (source_id, roi_version, ref_width, ref_height, vertices_json, buffer_distance, min_confirm_frames, invert_direction) "
              "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(QString::fromStdString(src_id));
    q.addBindValue(ver);
    q.addBindValue(config.ref_width > 0 ? config.ref_width : 1920);
    q.addBindValue(config.ref_height > 0 ? config.ref_height : 1080);
    q.addBindValue(json_str);
    q.addBindValue(config.buffer_distance);
    q.addBindValue(config.min_confirm_frames);
    q.addBindValue(config.invert_direction ? 1 : 0);
    return q.exec();
}

std::vector<RoiConfig> DatabaseManager::load_latest_roi_configs() {
    std::vector<RoiConfig> list;
    QSqlDatabase db = get_database();
    QSqlQuery q(db);
    q.prepare("SELECT source_id, roi_version, ref_width, ref_height, vertices_json, buffer_distance, min_confirm_frames, invert_direction "
              "FROM roi_configs ORDER BY id DESC LIMIT 1");
    if (q.exec() && q.next()) {
        RoiConfig cfg;
        cfg.source_id = q.value(0).toString().toStdString();
        cfg.version = q.value(1).toInt();
        cfg.ref_width = q.value(2).toInt();
        cfg.ref_height = q.value(3).toInt();

        QByteArray bytes = q.value(4).toString().toUtf8();
        QJsonDocument doc = QJsonDocument::fromJson(bytes);
        if (doc.isArray()) {
            QJsonArray arr = doc.array();
            for (const auto& item : arr) {
                QJsonObject obj = item.toObject();
                cfg.vertices.emplace_back(obj["x"].toDouble(), obj["y"].toDouble());
            }
        }
        cfg.buffer_distance = q.value(5).toFloat();
        cfg.min_confirm_frames = q.value(6).toInt() > 0 ? q.value(6).toInt() : 4;
        cfg.invert_direction = (q.value(7).toInt() != 0);
        cfg.is_valid = (cfg.vertices.size() >= 3);
        list.push_back(cfg);
    }

    // 若库中尚无任何标定记录，开箱即用自动生成并存入首选默认地面通道防区
    if (list.empty()) {
        RoiConfig default_roi = make_default_corridor_roi(1920, 1080);
        save_roi_config(default_roi);
        list.push_back(default_roi);
    }

    return list;
}

void DatabaseManager::get_today_counts(int& out_entry, int& out_exit) {
    out_entry = 0;
    out_exit = 0;
    QDateTime now = QDateTime::currentDateTime();
    int64_t start_ms = QDateTime(now.date(), QTime(0, 0, 0)).toMSecsSinceEpoch();

    QSqlDatabase db = get_database();
    QSqlQuery q(db);
    q.prepare("SELECT direction, COUNT(*) FROM passage_events WHERE timestamp_ms >= ? GROUP BY direction");
    q.addBindValue(static_cast<qint64>(start_ms));
    if (q.exec()) {
        while (q.next()) {
            int dir = q.value(0).toInt();
            int cnt = q.value(1).toInt();
            if (dir == static_cast<int>(PassageDirection::ENTRY)) {
                out_entry = cnt;
            } else if (dir == static_cast<int>(PassageDirection::EXIT)) {
                out_exit = cnt;
            }
        }
    }
}

