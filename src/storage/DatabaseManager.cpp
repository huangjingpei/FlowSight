#include "DatabaseManager.hpp"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>

DatabaseManager& DatabaseManager::instance() {
    static DatabaseManager inst;
    return inst;
}

bool DatabaseManager::init_database(const QString& db_path) {
    QFileInfo fi(db_path);
    QDir dir = fi.dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(db_path);

    if (!db.open()) {
        qCritical() << "[DatabaseManager] Failed to open SQLite:" << db.lastError().text();
        return false;
    }

    QSqlQuery q;
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
        "  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ");"
    );

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

    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    QSqlQuery q;
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

std::vector<PassageEvent> DatabaseManager::query_recent_events(int limit) {
    std::vector<PassageEvent> list;
    QSqlQuery q;
    q.prepare("SELECT timestamp_ms, track_id, direction, roi_version, confidence, source_id, clip_path, gender, age_group "
              "FROM passage_events ORDER BY timestamp_ms DESC LIMIT ?");
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

std::vector<PassageEvent> DatabaseManager::query_events_by_timerange(int64_t start_ms, int64_t end_ms, int limit) {
    std::vector<PassageEvent> list;
    QSqlQuery q;
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
    QJsonArray arr;
    for (const auto& pt : config.vertices) {
        QJsonObject obj;
        obj["x"] = pt.x;
        obj["y"] = pt.y;
        arr.append(obj);
    }
    QJsonDocument doc(arr);
    QString json_str = doc.toJson(QJsonDocument::Compact);

    QSqlQuery q;
    q.prepare("INSERT INTO roi_configs (source_id, roi_version, ref_width, ref_height, vertices_json, buffer_distance) "
              "VALUES (?, ?, ?, ?, ?, ?)");
    q.addBindValue(QString::fromStdString(config.source_id));
    q.addBindValue(config.version);
    q.addBindValue(config.ref_width);
    q.addBindValue(config.ref_height);
    q.addBindValue(json_str);
    q.addBindValue(config.buffer_distance);
    return q.exec();
}

std::vector<RoiConfig> DatabaseManager::load_latest_roi_configs() {
    std::vector<RoiConfig> list;
    QSqlQuery q("SELECT source_id, roi_version, ref_width, ref_height, vertices_json, buffer_distance "
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
        cfg.is_valid = (cfg.vertices.size() >= 3);
        list.push_back(cfg);
    }
    return list;
}
