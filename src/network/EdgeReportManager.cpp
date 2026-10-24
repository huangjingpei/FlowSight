#include "EdgeReportManager.hpp"
#include "storage/ConfigManager.hpp"
#include "storage/DatabaseManager.hpp"
#include <QJsonDocument>
#include <QDateTime>
#include <QUuid>
#include <QElapsedTimer>
#include <QDebug>
#include <QSysInfo>

EdgeReportManager& EdgeReportManager::instance() {
    static EdgeReportManager inst;
    return inst;
}

EdgeReportManager::EdgeReportManager(QObject* parent) : QObject(parent) {
    client_name_ = QString("FlowSight-%1").arg(QSysInfo::machineHostName());
}

void EdgeReportManager::init(QObject* parent) {
    if (parent && this->parent() != parent) {
        setParent(parent);
    }

    if (!nam_) {
        nam_ = new QNetworkAccessManager(this);
    }

    auto& cfg = ConfigManager::instance();
    cloud_base_url_ = cfg.get_webhook_url();
    bound_point_token_ = cfg.get_webhook_token();

    if (!batch_timer_) {
        batch_timer_ = new QTimer(this);
        connect(batch_timer_, &QTimer::timeout, this, &EdgeReportManager::on_batch_timer_timeout);
        batch_timer_->start(window_seconds_ * 1000);
    }

    if (!watchdog_timer_) {
        watchdog_timer_ = new QTimer(this);
        connect(watchdog_timer_, &QTimer::timeout, this, &EdgeReportManager::on_watchdog_timer_timeout);
        watchdog_timer_->start(15000); // 每 15 秒探活一次
    }

    window_start_ts_ = QDateTime::currentMSecsSinceEpoch();
}

void EdgeReportManager::login_with_credentials(const QString& base_url, const QString& phone, const QString& password, LoginCallback callback) {
    if (!nam_) nam_ = new QNetworkAccessManager(this);

    QUrl login_url(base_url.trimmed() + "/api/v1/auth/login");
    QNetworkRequest request(login_url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject obj;
    obj["phone"] = phone.trimmed();
    obj["password"] = password.trimmed();
    QByteArray body = QJsonDocument(obj).toJson(QJsonDocument::Compact);

    QNetworkReply* reply = nam_->post(request, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, base_url, callback]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            if (callback) callback(false, QString("登录网络异常: %1").arg(reply->errorString()), QJsonArray());
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject root = doc.object();
        int code = root["code"].toInt();
        if (code != 200) {
            if (callback) callback(false, root["message"].toString("登录认证失败"), QJsonArray());
            return;
        }

        QJsonObject data = root["data"].toObject();
        QString token = data["token"].toString();

        // 登录成功后，以颁发的 JWT 凭据获取该手机账号关联的点位列表
        QUrl points_url(base_url.trimmed() + "/api/v1/h5/points");
        QNetworkRequest p_req(points_url);
        p_req.setRawHeader("Authorization", "Bearer " + token.toUtf8());

        QNetworkReply* p_reply = nam_->get(p_req);
        connect(p_reply, &QNetworkReply::finished, this, [p_reply, callback]() {
            p_reply->deleteLater();
            if (p_reply->error() != QNetworkReply::NoError) {
                if (callback) callback(false, "获取点位列表失败", QJsonArray());
                return;
            }
            QJsonDocument p_doc = QJsonDocument::fromJson(p_reply->readAll());
            QJsonObject p_root = p_doc.object();
            QJsonArray points = p_root["data"].toArray();
            if (callback) callback(true, "登录成功", points);
        });
    });
}

void EdgeReportManager::bind_point(const QString& base_url, qint64 point_id, const QString& point_name, const QString& point_token, const QString& client_name) {
    cloud_base_url_ = base_url.trimmed();
    bound_point_id_ = point_id;
    bound_point_name_ = point_name;
    bound_point_token_ = point_token;
    if (!client_name.trimmed().isEmpty()) {
        client_name_ = client_name.trimmed();
    }

    auto& cfg = ConfigManager::instance();
    cfg.set_webhook_url(cloud_base_url_);
    cfg.set_webhook_token(bound_point_token_);
    cfg.set_webhook_enabled(true);
    cfg.save();

    is_online_ = true;
    emit cloud_status_changed(true, QString("已绑定点位: %1 [在线]").arg(bound_point_name_));
}

void EdgeReportManager::record_passage(const PassageEvent& event) {
    QMutexLocker locker(&mutex_);
    if (event.direction == PassageDirection::ENTRY) {
        in_increment_++;
    } else {
        out_increment_++;
    }

    if (event.gender == "男") male_count_++;
    else if (event.gender == "女") female_count_++;

    if (event.age_group.find("儿童") != std::string::npos) child_count_++;
    else if (event.age_group.find("老年") != std::string::npos) senior_count_++;
    else adult_count_++;

    batch_events_.push_back(event);

    // 内存缓冲达到阈值 (50 条) 立即提前刷新
    if (batch_events_.size() >= 50) {
        locker.unlock();
        flush_batch();
    }
}

QByteArray EdgeReportManager::compress_data(const QByteArray& raw_json) {
    // 使用 Qt 内置 qCompress，并剔除前 4 字节的 Qt 私有 length header，输出标准 Deflate/ZLIB 流
    QByteArray compressed = qCompress(raw_json);
    if (compressed.size() > 4) {
        return compressed.mid(4);
    }
    return raw_json;
}

void EdgeReportManager::flush_batch() {
    QMutexLocker locker(&mutex_);
    if (batch_events_.empty() && in_increment_ == 0 && out_increment_ == 0) {
        return;
    }

    qint64 now_ts = QDateTime::currentMSecsSinceEpoch();
    QString batch_uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QJsonObject root;
    root["batchUuid"] = batch_uuid;
    root["clientName"] = client_name_;
    root["appVersion"] = "v1.2.0";
    root["pointId"] = bound_point_id_;
    root["startTimeMs"] = window_start_ts_;
    root["endTimeMs"] = now_ts;

    QJsonObject headcount;
    headcount["inIncrement"] = in_increment_;
    headcount["outIncrement"] = out_increment_;
    headcount["currentOccupancy"] = current_occupancy_;
    root["headcount"] = headcount;

    QJsonObject demographics;
    demographics["maleCount"] = male_count_;
    demographics["femaleCount"] = female_count_;
    demographics["childCount"] = child_count_;
    demographics["adultCount"] = adult_count_;
    demographics["seniorCount"] = senior_count_;
    root["demographics"] = demographics;

    QJsonObject telemetry;
    telemetry["fps"] = 25.0;
    telemetry["inferLatencyMs"] = 18.5;
    telemetry["hardwareBackend"] = "CPU";
    telemetry["pendingOfflineQueue"] = DatabaseManager::instance().get_pending_offline_count();
    root["telemetry"] = telemetry;

    QJsonArray events_arr;
    for (const auto& ev : batch_events_) {
        QJsonObject ev_obj;
        ev_obj["trackId"] = ev.track_id;
        ev_obj["direction"] = (ev.direction == PassageDirection::ENTRY) ? "ENTRY" : "EXIT";
        ev_obj["confidence"] = static_cast<double>(ev.confidence);
        ev_obj["timestamp"] = static_cast<qint64>(ev.timestamp_ms);
        ev_obj["clipPath"] = QString::fromStdString(ev.clip_path);
        QJsonObject attrs;
        attrs["gender"] = QString::fromStdString(ev.gender);
        attrs["age_group"] = QString::fromStdString(ev.age_group);
        ev_obj["attributes"] = attrs;
        events_arr.append(ev_obj);
    }
    root["events"] = events_arr;

    int total_events = static_cast<int>(batch_events_.size());

    // 重置内存聚合池
    in_increment_ = 0;
    out_increment_ = 0;
    male_count_ = 0;
    female_count_ = 0;
    child_count_ = 0;
    adult_count_ = 0;
    senior_count_ = 0;
    batch_events_.clear();
    window_start_ts_ = now_ts;

    locker.unlock();

    // 序列化与 GZIP 压缩
    QByteArray raw_json = QJsonDocument(root).toJson(QJsonDocument::Compact);
    QByteArray compressed_blob = compress_data(raw_json);

    if (cloud_base_url_.trimmed().isEmpty() || bound_point_token_.trimmed().isEmpty()) {
        // 未配置云端，直接转存离线队列表
        DatabaseManager::instance().enqueue_offline_report(batch_uuid, "METRICS_BATCH", compressed_blob);
        emit offline_queue_updated(DatabaseManager::instance().get_pending_offline_count());
        return;
    }

    QUrl upload_url(cloud_base_url_.trimmed() + "/api/v1/edge/metrics/batch");
    post_compressed_json(upload_url, bound_point_token_, compressed_blob, batch_uuid,
        [this, batch_uuid, compressed_blob, total_events](bool success, int code, const QString& err) {
            if (success) {
                is_online_ = true;
                emit cloud_status_changed(true, "云端同步正常 (HTTP 200)");
                emit batch_reported(true, total_events, 0);
            } else {
                // 上传失败: 网络抖动/断网/5xx，保存到本地 SQLite 离线队列
                qWarning().noquote() << QString("[EdgeReport] Upload failed (%1, code %2). Saving to SQLite offline queue.")
                                            .arg(err).arg(code);
                DatabaseManager::instance().enqueue_offline_report(batch_uuid, "METRICS_BATCH", compressed_blob);
                is_online_ = false;
                emit cloud_status_changed(false, QString("网络中断，已转存本地离线队列 (%1)").arg(err));
                emit offline_queue_updated(DatabaseManager::instance().get_pending_offline_count());
            }
        });
}

void EdgeReportManager::post_compressed_json(const QUrl& url, const QString& token, const QByteArray& compressed_blob,
                                            const QString& batch_uuid, std::function<void(bool success, int code, const QString& err)> callback) {
    if (!nam_) nam_ = new QNetworkAccessManager(this);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Content-Encoding", "gzip");
    request.setRawHeader("X-Point-Token", token.toUtf8());
    request.setRawHeader("X-Batch-Uuid", batch_uuid.toUtf8());
    request.setTransferTimeout(4000); // 4秒超时，避免阻塞

    auto* timer = new QElapsedTimer();
    timer->start();

    QNetworkReply* reply = nam_->post(request, compressed_blob);
    connect(reply, &QNetworkReply::finished, this, [this, reply, timer, callback]() {
        delete timer;
        int status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        bool success = (reply->error() == QNetworkReply::NoError && status_code == 200);
        QString err = reply->errorString();
        reply->deleteLater();

        if (callback) {
            callback(success, status_code, err);
        }
    });
}

void EdgeReportManager::on_batch_timer_timeout() {
    flush_batch();
}

void EdgeReportManager::on_watchdog_timer_timeout() {
    int pending = DatabaseManager::instance().get_pending_offline_count();
    emit offline_queue_updated(pending);

    if (pending > 0 && !is_draining_) {
        drain_offline_queue();
    }
}

void EdgeReportManager::drain_offline_queue() {
    if (cloud_base_url_.trimmed().isEmpty() || bound_point_token_.trimmed().isEmpty()) return;

    auto items = DatabaseManager::instance().fetch_pending_offline_reports(3);
    if (items.empty()) return;

    is_draining_ = true;
    auto item = items.front();

    QUrl upload_url(cloud_base_url_.trimmed() + "/api/v1/edge/metrics/batch");
    post_compressed_json(upload_url, bound_point_token_, item.payload_blob, item.batch_uuid,
        [this, item](bool success, int code, const QString& err) {
            is_draining_ = false;
            if (success) {
                // 上传成功，从 SQLite 离线队列中删除
                DatabaseManager::instance().remove_offline_report(item.queue_id);
                qInfo().noquote() << QString("[EdgeReport] Successfully drained offline queue #%1 (UUID: %2)")
                                        .arg(item.queue_id).arg(item.batch_uuid);
                emit offline_queue_updated(DatabaseManager::instance().get_pending_offline_count());
                // 继续排空下一条
                drain_offline_queue();
            } else {
                // 失败，增加重试计数并退避
                DatabaseManager::instance().increment_offline_retry(item.queue_id);
                qWarning().noquote() << QString("[EdgeReport] Failed to drain offline queue #%1: %2").arg(item.queue_id).arg(err);
            }
        });
}

void EdgeReportManager::send_heartbeat(float fps, float latency_ms, int current_occupancy, const QString& backend) {
    current_occupancy_ = current_occupancy;
    if (cloud_base_url_.trimmed().isEmpty() || bound_point_token_.trimmed().isEmpty()) return;

    QUrl hb_url(cloud_base_url_.trimmed() + "/api/v1/edge/heartbeat");
    QNetworkRequest request(hb_url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("X-Point-Token", bound_point_token_.toUtf8());
    request.setTransferTimeout(3000);

    QJsonObject obj;
    obj["clientName"] = client_name_;
    obj["clientVersion"] = "v1.2.0";
    obj["fps"] = static_cast<double>(fps);
    obj["inferLatencyMs"] = static_cast<double>(latency_ms);
    obj["hardwareBackend"] = backend;
    obj["currentOccupancy"] = current_occupancy;

    QByteArray body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    QNetworkReply* reply = nam_->post(request, body);
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}

int EdgeReportManager::get_offline_queue_count() const {
    return DatabaseManager::instance().get_pending_offline_count();
}
