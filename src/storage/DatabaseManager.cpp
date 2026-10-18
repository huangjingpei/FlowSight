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
    q.exec("ALTER TABLE roi_configs ADD COLUMN roi_id INTEGER DEFAULT 1;");
    q.exec("ALTER TABLE roi_configs ADD COLUMN name TEXT DEFAULT 'A门主通道';");
    q.exec("ALTER TABLE roi_configs ADD COLUMN color_hex TEXT DEFAULT '#58a6ff';");

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
    q.exec("ALTER TABLE passage_events ADD COLUMN roi_id INTEGER DEFAULT 1;");
    q.exec("ALTER TABLE passage_events ADD COLUMN roi_name TEXT DEFAULT 'A门主通道';");
    q.exec("CREATE INDEX IF NOT EXISTS idx_events_ts ON passage_events(timestamp_ms);");

    // 3. 小时级后台预聚合加速表 (P1-6)
    q.exec(
        "CREATE TABLE IF NOT EXISTS stats_hourly ("
        "  date_hour TEXT PRIMARY KEY,"
        "  date TEXT NOT NULL,"
        "  hour INTEGER NOT NULL,"
        "  entry_count INTEGER DEFAULT 0,"
        "  exit_count INTEGER DEFAULT 0,"
        "  total_count INTEGER DEFAULT 0,"
        "  peak_occupancy INTEGER DEFAULT 0,"
        "  updated_at INTEGER NOT NULL"
        ");"
    );
    q.exec("CREATE INDEX IF NOT EXISTS idx_stats_hourly_date ON stats_hourly(date);");
    q.exec("CREATE INDEX IF NOT EXISTS idx_stats_hourly_hour ON stats_hourly(hour);");

    // 启动时自动历史自检与极速回填
    sync_hourly_aggregation();

    return true;
}

bool DatabaseManager::insert_events(const std::vector<PassageEvent>& events) {
    if (events.empty()) return true;

    QSqlDatabase db = get_database();
    if (!db.isOpen()) return false;
    db.transaction();

    QSqlQuery q(db);
    q.prepare("INSERT INTO passage_events ("
              "source_id, timestamp_ms, track_id, direction, roi_version, confidence, clip_path, gender, age_group, roi_id, roi_name) "
              "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

    struct HourlyDelta {
        QString date_str;
        int hour = 0;
        int entry = 0;
        int exit = 0;
        int total = 0;
    };
    std::map<QString, HourlyDelta> hourly_deltas;

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
        q.addBindValue(ev.roi_id > 0 ? ev.roi_id : 1);
        q.addBindValue(QString::fromStdString(ev.roi_name.empty() ? "A门主通道" : ev.roi_name));
        q.exec();

        // 累计本批次对应小时段的增量
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(ev.timestamp_ms);
        QString dh = dt.toString("yyyy-MM-dd hh:00");
        auto& d = hourly_deltas[dh];
        d.date_str = dt.toString("yyyy-MM-dd");
        d.hour = dt.time().hour();
        if (ev.direction == PassageDirection::ENTRY) {
            d.entry++;
        } else if (ev.direction == PassageDirection::EXIT) {
            d.exit++;
        }
        d.total++;
    }

    // 增量 Upsert 写入 stats_hourly
    qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
    for (const auto& [dh, d] : hourly_deltas) {
        QSqlQuery q_agg(db);
        q_agg.prepare(
            "INSERT INTO stats_hourly (date_hour, date, hour, entry_count, exit_count, total_count, peak_occupancy, updated_at) "
            "VALUES (?, ?, ?, ?, ?, ?, 0, ?) "
            "ON CONFLICT(date_hour) DO UPDATE SET "
            "  entry_count = entry_count + excluded.entry_count, "
            "  exit_count = exit_count + excluded.exit_count, "
            "  total_count = total_count + excluded.total_count, "
            "  updated_at = excluded.updated_at;"
        );
        q_agg.addBindValue(dh);
        q_agg.addBindValue(d.date_str);
        q_agg.addBindValue(d.hour);
        q_agg.addBindValue(d.entry);
        q_agg.addBindValue(d.exit);
        q_agg.addBindValue(d.total);
        q_agg.addBindValue(now_ms);
        q_agg.exec();
    }

    return db.commit();
}

std::vector<PassageEvent> DatabaseManager::query_filtered_events(int direction_filter, int64_t start_ms, int64_t end_ms, const QString& keyword, int limit) {
    std::vector<PassageEvent> list;
    QString sql = "SELECT timestamp_ms, track_id, direction, roi_version, confidence, source_id, clip_path, gender, age_group, roi_id, roi_name "
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
        sql += " AND (CAST(track_id AS TEXT) LIKE ? OR clip_path LIKE ? OR gender LIKE ? OR age_group LIKE ? OR roi_name LIKE ?)";
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
            ev.roi_id = q.value(9).toInt() > 0 ? q.value(9).toInt() : 1;
            ev.roi_name = q.value(10).toString().toStdString();
            if (ev.roi_name.empty()) ev.roi_name = "A门主通道";
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
    q.prepare("SELECT timestamp_ms, track_id, direction, roi_version, confidence, source_id, clip_path, gender, age_group, roi_id, roi_name "
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
            ev.roi_id = q.value(9).toInt() > 0 ? q.value(9).toInt() : 1;
            ev.roi_name = q.value(10).toString().toStdString();
            if (ev.roi_name.empty()) ev.roi_name = "A门主通道";
            list.push_back(ev);
        }
    }
    return list;
}

bool DatabaseManager::save_roi_config(const RoiConfig& config) {
    return save_roi_configs({config});
}

bool DatabaseManager::save_roi_configs(const std::vector<RoiConfig>& configs) {
    if (configs.empty()) return false;

    QSqlDatabase db = get_database();
    if (!db.isOpen()) return false;

    // 确定全局递增的新版本号
    int next_ver = 1;
    QSqlQuery q_ver(db);
    if (q_ver.exec("SELECT MAX(roi_version) FROM roi_configs") && q_ver.next()) {
        next_ver = q_ver.value(0).toInt() + 1;
    }

    db.transaction();
    for (const auto& config : configs) {
        if (config.vertices.size() < 3) continue;

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
        int r_id = config.roi_id > 0 ? config.roi_id : 1;
        QString r_name = QString::fromStdString(config.name.empty() ? "A门主通道" : config.name);
        QString r_color = QString::fromStdString(config.color_hex.empty() ? "#58a6ff" : config.color_hex);

        QSqlQuery q(db);
        q.prepare("INSERT INTO roi_configs (source_id, roi_version, ref_width, ref_height, vertices_json, buffer_distance, min_confirm_frames, invert_direction, roi_id, name, color_hex) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
        q.addBindValue(QString::fromStdString(src_id));
        q.addBindValue(next_ver);
        q.addBindValue(config.ref_width > 0 ? config.ref_width : 1920);
        q.addBindValue(config.ref_height > 0 ? config.ref_height : 1080);
        q.addBindValue(json_str);
        q.addBindValue(config.buffer_distance);
        q.addBindValue(config.min_confirm_frames);
        q.addBindValue(config.invert_direction ? 1 : 0);
        q.addBindValue(r_id);
        q.addBindValue(r_name);
        q.addBindValue(r_color);
        q.exec();
    }

    return db.commit();
}

std::vector<RoiConfig> DatabaseManager::load_latest_roi_configs() {
    std::vector<RoiConfig> list;
    QSqlDatabase db = get_database();

    int latest_ver = 0;
    QSqlQuery q_ver(db);
    if (q_ver.exec("SELECT roi_version FROM roi_configs ORDER BY id DESC LIMIT 1") && q_ver.next()) {
        latest_ver = q_ver.value(0).toInt();
    }

    if (latest_ver > 0) {
        QSqlQuery q(db);
        q.prepare("SELECT source_id, roi_version, ref_width, ref_height, vertices_json, buffer_distance, min_confirm_frames, invert_direction, roi_id, name, color_hex "
                  "FROM roi_configs WHERE roi_version = ? ORDER BY roi_id ASC");
        q.addBindValue(latest_ver);
        if (q.exec()) {
            while (q.next()) {
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
                cfg.roi_id = q.value(8).toInt() > 0 ? q.value(8).toInt() : 1;
                cfg.name = q.value(9).toString().toStdString();
                if (cfg.name.empty()) cfg.name = "A门主通道";
                cfg.color_hex = q.value(10).toString().toStdString();
                if (cfg.color_hex.empty()) cfg.color_hex = "#58a6ff";

                cfg.is_valid = (cfg.vertices.size() >= 3);
                list.push_back(cfg);
            }
        }
    }

    // 若库中尚无任何标定记录，开箱即用自动生成并存入首选默认地面通道防区
    if (list.empty()) {
        RoiConfig default_roi = make_default_corridor_roi(1920, 1080);
        save_roi_configs({default_roi});
        list.push_back(default_roi);
    }

    return list;
}

void DatabaseManager::get_today_counts(int& out_entry, int& out_exit) {
    out_entry = 0;
    out_exit = 0;
    QString today_str = QDateTime::currentDateTime().toString("yyyy-MM-dd");

    QSqlDatabase db = get_database();
    if (!db.isOpen()) return;

    // 优先直接查询 stats_hourly 小时预聚合加速表 (至多扫描 24 行，< 0.1ms)
    QSqlQuery q(db);
    q.prepare("SELECT SUM(entry_count), SUM(exit_count) FROM stats_hourly WHERE date = ?");
    q.addBindValue(today_str);
    if (q.exec() && q.next()) {
        if (!q.value(0).isNull() || !q.value(1).isNull()) {
            out_entry = q.value(0).toInt();
            out_exit = q.value(1).toInt();
            return;
        }
    }

    // 兜底回退：若 stats_hourly 无数据则查询原始明细
    QDateTime now = QDateTime::currentDateTime();
    int64_t start_ms = QDateTime(now.date(), QTime(0, 0, 0)).toMSecsSinceEpoch();
    QSqlQuery q_fallback(db);
    q_fallback.prepare("SELECT direction, COUNT(*) FROM passage_events WHERE timestamp_ms >= ? GROUP BY direction");
    q_fallback.addBindValue(static_cast<qint64>(start_ms));
    if (q_fallback.exec()) {
        while (q_fallback.next()) {
            int dir = q_fallback.value(0).toInt();
            int cnt = q_fallback.value(1).toInt();
            if (dir == static_cast<int>(PassageDirection::ENTRY)) {
                out_entry = cnt;
            } else if (dir == static_cast<int>(PassageDirection::EXIT)) {
                out_exit = cnt;
            }
        }
    }
}

bool DatabaseManager::sync_hourly_aggregation() {
    QSqlDatabase db = get_database();
    if (!db.isOpen()) return false;

    int agg_count = 0;
    int event_count = 0;

    QSqlQuery q(db);
    if (q.exec("SELECT COUNT(*) FROM stats_hourly") && q.next()) {
        agg_count = q.value(0).toInt();
    }
    if (q.exec("SELECT COUNT(*) FROM passage_events") && q.next()) {
        event_count = q.value(0).toInt();
    }

    // 若已有预聚合数据，或流水表为空，则无须全量重算
    if (agg_count > 0 || event_count == 0) {
        return true;
    }

    qInfo() << "[DatabaseManager] stats_hourly is empty but passage_events has" << event_count << "records. Starting auto-backfill...";

    db.transaction();
    QSqlQuery q_all(db);
    if (q_all.exec("SELECT timestamp_ms, direction FROM passage_events")) {
        struct HourlyBucket {
            QString date_str;
            int hour = 0;
            int entry = 0;
            int exit = 0;
            int total = 0;
        };
        std::map<QString, HourlyBucket> buckets;

        while (q_all.next()) {
            qint64 ts = q_all.value(0).toLongLong();
            int dir = q_all.value(1).toInt();
            QDateTime dt = QDateTime::fromMSecsSinceEpoch(ts);
            QString dh = dt.toString("yyyy-MM-dd hh:00");
            auto& b = buckets[dh];
            b.date_str = dt.toString("yyyy-MM-dd");
            b.hour = dt.time().hour();
            if (dir == static_cast<int>(PassageDirection::ENTRY)) {
                b.entry++;
            } else if (dir == static_cast<int>(PassageDirection::EXIT)) {
                b.exit++;
            }
            b.total++;
        }

        qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
        for (const auto& [dh, b] : buckets) {
            QSqlQuery q_ins(db);
            q_ins.prepare(
                "INSERT INTO stats_hourly (date_hour, date, hour, entry_count, exit_count, total_count, peak_occupancy, updated_at) "
                "VALUES (?, ?, ?, ?, ?, ?, 0, ?) "
                "ON CONFLICT(date_hour) DO UPDATE SET "
                "  entry_count = excluded.entry_count, "
                "  exit_count = excluded.exit_count, "
                "  total_count = excluded.total_count, "
                "  updated_at = excluded.updated_at;"
            );
            q_ins.addBindValue(dh);
            q_ins.addBindValue(b.date_str);
            q_ins.addBindValue(b.hour);
            q_ins.addBindValue(b.entry);
            q_ins.addBindValue(b.exit);
            q_ins.addBindValue(b.total);
            q_ins.addBindValue(now_ms);
            q_ins.exec();
        }
        db.commit();
        qInfo() << "[DatabaseManager] Successfully backfilled" << buckets.size() << "hourly buckets into stats_hourly.";
        return true;
    }

    db.rollback();
    return false;
}

std::vector<HourlyStat> DatabaseManager::query_hourly_stats(int64_t start_ms, int64_t end_ms) {
    std::vector<HourlyStat> list;
    QSqlDatabase db = get_database();
    if (!db.isOpen()) return list;

    QString start_dh = QDateTime::fromMSecsSinceEpoch(start_ms).toString("yyyy-MM-dd hh:00");
    QString end_dh = QDateTime::fromMSecsSinceEpoch(end_ms).toString("yyyy-MM-dd hh:59");

    QSqlQuery q(db);
    q.prepare(
        "SELECT date_hour, date, hour, entry_count, exit_count, total_count, peak_occupancy, updated_at "
        "FROM stats_hourly "
        "WHERE date_hour >= ? AND date_hour <= ? "
        "ORDER BY date_hour ASC;"
    );
    q.addBindValue(start_dh);
    q.addBindValue(end_dh);

    if (q.exec()) {
        while (q.next()) {
            HourlyStat s;
            s.date_hour = q.value(0).toString().toStdString();
            s.date = q.value(1).toString().toStdString();
            s.hour = q.value(2).toInt();
            s.entry_count = q.value(3).toInt();
            s.exit_count = q.value(4).toInt();
            s.total_count = q.value(5).toInt();
            s.peak_occupancy = q.value(6).toInt();
            s.updated_at = q.value(7).toLongLong();
            list.push_back(s);
        }
    }
    return list;
}

std::map<int, std::pair<int, int>> DatabaseManager::query_hourly_distribution(int64_t start_ms, int64_t end_ms) {
    std::map<int, std::pair<int, int>> result;
    for (int h = 0; h < 24; ++h) {
        result[h] = {0, 0};
    }

    QSqlDatabase db = get_database();
    if (!db.isOpen()) return result;

    QString start_dh = QDateTime::fromMSecsSinceEpoch(start_ms).toString("yyyy-MM-dd hh:00");
    QString end_dh = QDateTime::fromMSecsSinceEpoch(end_ms).toString("yyyy-MM-dd hh:59");

    QSqlQuery q(db);
    q.prepare(
        "SELECT hour, SUM(entry_count), SUM(exit_count) "
        "FROM stats_hourly "
        "WHERE date_hour >= ? AND date_hour <= ? "
        "GROUP BY hour ORDER BY hour ASC;"
    );
    q.addBindValue(start_dh);
    q.addBindValue(end_dh);

    if (q.exec()) {
        while (q.next()) {
            int h = q.value(0).toInt();
            int entry = q.value(1).toInt();
            int exit = q.value(2).toInt();
            if (h >= 0 && h < 24) {
                result[h] = {entry, exit};
            }
        }
    }
    return result;
}

void DatabaseManager::get_demographics(int64_t start_ms, int64_t end_ms, int& out_male, int& out_female) {
    out_male = 0;
    out_female = 0;
    QSqlDatabase db = get_database();
    if (!db.isOpen()) return;

    QSqlQuery q(db);
    q.prepare("SELECT gender, COUNT(*) FROM passage_events WHERE timestamp_ms >= ? AND timestamp_ms <= ? GROUP BY gender");
    q.addBindValue(static_cast<qint64>(start_ms));
    q.addBindValue(static_cast<qint64>(end_ms));
    if (q.exec()) {
        while (q.next()) {
            QString g = q.value(0).toString();
            int cnt = q.value(1).toInt();
            if (g == "男") {
                out_male = cnt;
            } else if (g == "女") {
                out_female = cnt;
            }
        }
    }
}


