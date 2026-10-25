#include "EdgeReportManager.hpp"
#include "storage/ConfigManager.hpp"
#include "storage/DatabaseManager.hpp"
#include <QJsonDocument>
#include <QDateTime>
#include <QUuid>
#include <QElapsedTimer>
#include <QDebug>
#include <QSysInfo>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QFileInfo>
#include <QFile>

EdgeReportManager& EdgeReportManager::instance() {
    static EdgeReportManager inst;
    return inst;
}

EdgeReportManager::EdgeReportManager(QObject* parent) : QObject(parent) {
    client_name_ = QString("FlowSight-%1").arg(QSysInfo::machineHostName());
}

void EdgeReportManager::init(QObject* parent) {
    Q_UNUSED(parent);
    // 注意：单例拥有全局 static 存储期，绝不能调用 setParent(parent)，否则当 MainWindow 析构时
    // QObject 会对 static 局部单例调用 delete 导致致命内存非法释放崩溃。

    if (!nam_) {
        nam_ = new QNetworkAccessManager(this);
    }

    auto& cfg = ConfigManager::instance();
    cloud_base_url_ = cfg.get_cloud_server_url();
    bound_point_id_ = cfg.get_cloud_point_id();
    bound_point_name_ = cfg.get_cloud_point_name();
    bound_point_token_ = cfg.get_cloud_point_token();
    client_name_ = cfg.get_cloud_client_name();

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

    if (!clip_upload_timer_) {
        clip_upload_timer_ = new QTimer(this);
        connect(clip_upload_timer_, &QTimer::timeout, this, &EdgeReportManager::process_clip_upload_queue);
        clip_upload_timer_->start(2000); // 每 2 秒轮询切片上云队列
    }

    window_start_ts_ = QDateTime::currentMSecsSinceEpoch();

    if (!cloud_base_url_.trimmed().isEmpty() && !bound_point_token_.trimmed().isEmpty()) {
        is_online_ = true;
        emit cloud_status_changed(true, QString("已绑定点位: %1 [在线]").arg(bound_point_name_));
    }
}

void EdgeReportManager::shutdown() {
    // 退出前主动向云端发送离线声明，使大屏/H5 0 秒无延迟感知设备已离线
    send_heartbeat(0.0f, 0.0f, 0, "CPU", "OFFLINE");

    if (batch_timer_) {
        batch_timer_->stop();
    }
    if (watchdog_timer_) {
        watchdog_timer_->stop();
    }
    if (clip_upload_timer_) {
        clip_upload_timer_->stop();
    }
    // 退出前将内存缓冲池中未上报的数据刷入 SQLite 本地持久化，确保 0 数据丢失
    flush_batch();

    {
        QMutexLocker locker(&clip_mutex_);
        pending_clips_.clear();
    }
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
        QString login_phone = data["phone"].toString();
        QString company_name = data["companyName"].toString();
        auto& cfg = ConfigManager::instance();
        if (!login_phone.isEmpty()) cfg.set_cloud_user_phone(login_phone);
        if (!company_name.isEmpty()) cfg.set_cloud_company_name(company_name);
        cfg.save();

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

void EdgeReportManager::login_with_device_credentials(const QString& base_url, const QString& point_code, const QString& device_password, const QString& client_name, DeviceLoginCallback callback) {
    if (!nam_) nam_ = new QNetworkAccessManager(this);

    QUrl dev_login_url(base_url.trimmed() + "/api/v1/edge/auth/device-login");
    QNetworkRequest request(dev_login_url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject obj;
    obj["pointCode"] = point_code.trimmed();
    obj["devicePassword"] = device_password.trimmed();
    obj["clientName"] = client_name.trimmed().isEmpty() ? "FlowSight-Node-1" : client_name.trimmed();
    QByteArray body = QJsonDocument(obj).toJson(QJsonDocument::Compact);

    QNetworkReply* reply = nam_->post(request, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, base_url, point_code, device_password, client_name, callback]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            if (callback) callback(false, QString("设备认证网络异常: %1").arg(reply->errorString()), 0, "", "");
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject root = doc.object();
        int code = root["code"].toInt();
        if (code != 200) {
            if (callback) callback(false, root["message"].toString("边缘设备专属密码错误或点位不存在"), 0, "", "");
            return;
        }

        QJsonObject data = root["data"].toObject();
        qint64 pid = data["pointId"].toInteger();
        QString pname = data["pointName"].toString();
        QString ptoken = data["pointToken"].toString();

        // 自动完成绑定与配置持久化
        auto& cfg = ConfigManager::instance();
        cfg.set_cloud_point_code(point_code.trimmed());
        cfg.set_cloud_device_password(device_password.trimmed());
        bind_point(base_url, pid, pname, ptoken, client_name);

        if (callback) callback(true, "设备身份认证成功！已接入云端通道。", pid, pname, ptoken);
    });
}

void EdgeReportManager::pair_with_code(const QString& base_url, const QString& pairing_code, const QString& client_name, PairingCallback callback) {
    if (!nam_) nam_ = new QNetworkAccessManager(this);

    QUrl pair_url(base_url.trimmed() + "/api/v1/edge/auth/pair-with-code");
    QNetworkRequest request(pair_url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject obj;
    obj["pairingCode"] = pairing_code.trimmed();
    obj["clientName"] = client_name.trimmed().isEmpty() ? "FlowSight-Node-1" : client_name.trimmed();
    QByteArray body = QJsonDocument(obj).toJson(QJsonDocument::Compact);

    QNetworkReply* reply = nam_->post(request, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, base_url, client_name, callback]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            if (callback) callback(false, QString("配对网络异常: %1").arg(reply->errorString()), 0, "", "");
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject root = doc.object();
        int code = root["code"].toInt();
        if (code != 200) {
            if (callback) callback(false, root["message"].toString("6位配对码无效或已过期"), 0, "", "");
            return;
        }

        QJsonObject data = root["data"].toObject();
        qint64 pid = data["pointId"].toInteger();
        QString pname = data["pointName"].toString();
        QString ptoken = data["pointToken"].toString();
        QString pcode = data["pointCode"].toString();

        // 自动完成绑定与持久化
        auto& cfg = ConfigManager::instance();
        if (!pcode.isEmpty()) {
            cfg.set_cloud_point_code(pcode);
        }
        bind_point(base_url, pid, pname, ptoken, client_name);

        if (callback) callback(true, "6位配对码接入成功！通道已绑定并上线。", pid, pname, ptoken);
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
    cfg.set_cloud_server_url(cloud_base_url_);
    cfg.set_cloud_point_id(bound_point_id_);
    cfg.set_cloud_point_name(bound_point_name_);
    cfg.set_cloud_point_token(bound_point_token_);
    cfg.set_cloud_client_name(client_name_);
    cfg.set_cloud_sync_enabled(true);
    cfg.save();

    is_online_ = true;
    is_point_deactivated_ = false;
    emit cloud_status_changed(true, QString("已绑定点位: %1 [在线]").arg(bound_point_name_));

    // 绑定生效后立即发送一次心跳同步状态
    send_heartbeat(30.0f, 15.0f, current_occupancy_, "CPU");
}

void EdgeReportManager::send_test_batch(std::function<void(bool success, const QString& msg, qint64 latency_ms)> callback) {
    if (cloud_base_url_.trimmed().isEmpty() || bound_point_token_.trimmed().isEmpty()) {
        if (callback) callback(false, "未配置云端地址或未绑定有效点位 Token", 0);
        return;
    }

    qint64 now_ts = QDateTime::currentMSecsSinceEpoch();
    QString batch_uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QJsonObject root;
    root["batchUuid"] = batch_uuid;
    root["clientName"] = client_name_;
    root["appVersion"] = "v1.2.0";
    root["pointId"] = bound_point_id_;
    root["startTimeMs"] = now_ts - 5000;
    root["endTimeMs"] = now_ts;

    QJsonObject headcount;
    headcount["inIncrement"] = 1;
    headcount["outIncrement"] = 0;
    headcount["currentOccupancy"] = current_occupancy_ + 1;
    root["headcount"] = headcount;

    QJsonObject demographics;
    demographics["maleCount"] = 1;
    demographics["femaleCount"] = 0;
    demographics["childCount"] = 0;
    demographics["adultCount"] = 1;
    demographics["seniorCount"] = 0;
    root["demographics"] = demographics;

    QJsonObject telemetry;
    telemetry["fps"] = 30.0;
    telemetry["inferLatencyMs"] = 14.5;
    telemetry["hardwareBackend"] = "CPU";
    telemetry["pendingOfflineQueue"] = DatabaseManager::instance().get_pending_offline_count();
    root["telemetry"] = telemetry;

    QJsonArray events_arr;
    QJsonObject ev_obj;
    ev_obj["trackId"] = 999;
    ev_obj["track_id"] = 999;
    ev_obj["direction"] = "ENTRY";
    ev_obj["confidence"] = 0.95;
    ev_obj["timestamp"] = now_ts;
    ev_obj["clipPath"] = "";
    ev_obj["clip_path"] = "";
    QJsonObject attrs;
    attrs["gender"] = "男";
    attrs["age_group"] = "青年(18-35)";
    ev_obj["attributes"] = attrs;
    events_arr.append(ev_obj);
    root["events"] = events_arr;

    QByteArray raw_json = QJsonDocument(root).toJson(QJsonDocument::Compact);
    QByteArray compressed_blob = compress_data(raw_json);

    QUrl upload_url(cloud_base_url_.trimmed() + "/api/v1/edge/metrics/batch");
    auto* start_timer = new QElapsedTimer();
    start_timer->start();

    post_compressed_json(upload_url, bound_point_token_, compressed_blob, batch_uuid,
        [this, start_timer, callback](bool success, int code, const QString& err) {
            qint64 latency = start_timer->elapsed();
            delete start_timer;
            if (success) {
                is_online_ = true;
                emit cloud_status_changed(true, QString("已绑定点位: %1 [在线]").arg(bound_point_name_));
                if (callback) callback(true, QString("测试聚合包上报成功 (HTTP %1)").arg(code), latency);
            } else {
                if (callback) callback(false, QString("测试上报失败: HTTP %1 (%2)").arg(code).arg(err), latency);
            }
        });
}

void EdgeReportManager::fetch_cloud_config(ConfigCallback callback) {
    if (cloud_base_url_.trimmed().isEmpty() || bound_point_token_.trimmed().isEmpty()) {
        if (callback) callback(false, "未配置云端中台地址或未绑定点位 Token", QJsonObject());
        return;
    }

    if (!nam_) nam_ = new QNetworkAccessManager(this);

    QUrl config_url(cloud_base_url_.trimmed() + "/api/v1/edge/config");
    QNetworkRequest request(config_url);
    request.setRawHeader("X-Point-Token", bound_point_token_.toUtf8());
    request.setTransferTimeout(5000);

    QNetworkReply* reply = nam_->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, callback]() {
        reply->deleteLater();
        int http_status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() == QNetworkReply::NoError && http_status == 200) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            QJsonObject root = doc.object();
            int code = root.value("code").toInt(200);
            if (code == 200 && root.contains("data")) {
                QJsonObject point_cfg = root.value("data").toObject();
                // 同步本地点位名称与属主商户账号、公司名称
                bound_point_name_ = point_cfg.value("pointName").toString(bound_point_name_);
                QString phone = point_cfg.value("userPhone").toString();
                QString company = point_cfg.value("companyName").toString();
                auto& cfg = ConfigManager::instance();
                if (!phone.isEmpty()) {
                    cfg.set_cloud_user_phone(phone);
                }
                if (!company.isEmpty()) {
                    cfg.set_cloud_company_name(company);
                }
                cfg.set_cloud_point_name(bound_point_name_);
                cfg.save();

                is_online_ = true;
                is_point_deactivated_ = false;
                emit cloud_status_changed(true, QString("已绑定点位: %1 [在线]").arg(bound_point_name_));
                if (callback) callback(true, "拉取云端点位配置成功", point_cfg);
                return;
            } else {
                QString msg = root.value("message").toString("云端返回异常");
                if (callback) callback(false, msg, QJsonObject());
                return;
            }
        }

        if (http_status == 400 || http_status == 401 || http_status == 403) {
            is_point_deactivated_ = true;
            is_online_ = false;
            emit cloud_status_changed(false, "⚠️ 点位已下架或Token已注销 (本地持续自主监控)");
        }
        if (callback) callback(false, QString("拉取失败 (HTTP %1): %2").arg(http_status).arg(reply->errorString()), QJsonObject());
    });
}

void EdgeReportManager::update_cloud_point_config(const QString& point_name, const QString& camera_rtsp, SyncCallback callback) {
    if (cloud_base_url_.trimmed().isEmpty() || bound_point_token_.trimmed().isEmpty()) {
        if (callback) callback(false, "未配置云端中台地址或未绑定点位");
        return;
    }

    if (!nam_) nam_ = new QNetworkAccessManager(this);

    QUrl update_url(cloud_base_url_.trimmed() + "/api/v1/edge/config");
    QNetworkRequest request(update_url);
    request.setRawHeader("X-Point-Token", bound_point_token_.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setTransferTimeout(5000);

    QJsonObject payload;
    if (!point_name.trimmed().isEmpty()) {
        payload["pointName"] = point_name.trimmed();
    }
    payload["cameraRtsp"] = camera_rtsp.trimmed();

    QNetworkReply* reply = nam_->sendCustomRequest(request, "PUT", QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, point_name, camera_rtsp, callback]() {
        reply->deleteLater();
        int http_status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() == QNetworkReply::NoError && (http_status == 200 || http_status == 204)) {
            if (!point_name.trimmed().isEmpty()) {
                bound_point_name_ = point_name.trimmed();
                ConfigManager::instance().set_cloud_point_name(bound_point_name_);
            }
            if (callback) callback(true, "点位配置已成功双向同步回写至云端中台！");
        } else {
            QString err_msg = QString("云端同步失败 (HTTP %1): %2").arg(http_status).arg(reply->errorString());
            if (callback) callback(false, err_msg);
        }
    });
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

    // 若事件生成了视频切片，投递至异步上传队列
    if (!event.clip_path.empty()) {
        enqueue_clip_upload(QString::fromStdString(event.clip_path), event.track_id, event.timestamp_ms);
    }

    // 内存缓冲达到阈值 (50 条) 立即提前刷新
    if (batch_events_.size() >= 50) {
        locker.unlock();
        flush_batch();
    }
}

void EdgeReportManager::update_event_attributes(int track_id, const std::string& gender, const std::string& age_group) {
    if (track_id <= 0 || gender.empty() || gender == "未知") return;

    QMutexLocker locker(&mutex_);
    for (auto& ev : batch_events_) {
        if (ev.track_id == track_id) {
            if (ev.gender != gender) {
                if (ev.gender == "男") male_count_ = std::max(0, male_count_ - 1);
                else if (ev.gender == "女") female_count_ = std::max(0, female_count_ - 1);

                if (gender == "男") male_count_++;
                else if (gender == "女") female_count_++;

                ev.gender = gender;
            }
            if (ev.age_group != age_group) {
                if (ev.age_group.find("儿童") != std::string::npos) child_count_ = std::max(0, child_count_ - 1);
                else if (ev.age_group.find("老年") != std::string::npos) senior_count_ = std::max(0, senior_count_ - 1);
                else adult_count_ = std::max(0, adult_count_ - 1);

                if (age_group.find("儿童") != std::string::npos) child_count_++;
                else if (age_group.find("老年") != std::string::npos) senior_count_++;
                else adult_count_++;

                ev.age_group = age_group;
            }
        }
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

void EdgeReportManager::reset_batch() {
    QMutexLocker locker(&mutex_);
    in_increment_ = 0;
    out_increment_ = 0;
    male_count_ = 0;
    female_count_ = 0;
    child_count_ = 0;
    adult_count_ = 0;
    senior_count_ = 0;
    batch_events_.clear();
    window_start_ts_ = QDateTime::currentMSecsSinceEpoch();

    QMutexLocker clip_locker(&clip_mutex_);
    pending_clips_.clear();
    qInfo() << "[EdgeReportManager] In-memory batch buffers and pending clip queue successfully reset.";
}

void EdgeReportManager::flush_batch() {
    QMutexLocker locker(&mutex_);
    if (batch_events_.empty() && in_increment_ == 0 && out_increment_ == 0) {
        return;
    }

    qint64 now_ts = QDateTime::currentMSecsSinceEpoch();

    if (is_point_deactivated_) {
        // 点位在云端已注销或 Token 失效，停止发送网络请求，重置内存池并保持本地自治
        in_increment_ = 0;
        out_increment_ = 0;
        male_count_ = 0;
        female_count_ = 0;
        child_count_ = 0;
        adult_count_ = 0;
        senior_count_ = 0;
        batch_events_.clear();
        window_start_ts_ = now_ts;
        return;
    }

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
        ev_obj["track_id"] = ev.track_id;
        ev_obj["direction"] = (ev.direction == PassageDirection::ENTRY) ? "ENTRY" : "EXIT";
        ev_obj["confidence"] = static_cast<double>(ev.confidence);
        ev_obj["timestamp"] = static_cast<qint64>(ev.timestamp_ms);
        ev_obj["clipPath"] = QString::fromStdString(ev.clip_path);
        ev_obj["clip_path"] = QString::fromStdString(ev.clip_path);
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
                is_point_deactivated_ = false;
                emit cloud_status_changed(true, "云端同步正常 (HTTP 200)");
                emit batch_reported(true, total_events, 0);
            } else if (code == 400 || code == 401 || code == 403) {
                // 点位在云端已删除、注销或 Token 已被管理员重置/作废！
                // 关键容灾机制: 严禁将死任务存入离线队列表，防止死循环无限重试！
                is_online_ = false;
                is_point_deactivated_ = true;
                qWarning().noquote() << QString("[EdgeReport] Point deactivated or token invalid (HTTP %1: %2). Entering local autonomous mode.")
                                            .arg(code).arg(err);
                emit cloud_status_changed(false, "⚠️ 点位已下架或Token已注销 (本地持续自主监控)");
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
    if (is_point_deactivated_) return;
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
            } else if (code == 400 || code == 401 || code == 403) {
                // 点位在云端已被删除或 Token 失效：直接剔除历史不可达死任务，彻底打破死循环！
                is_point_deactivated_ = true;
                is_online_ = false;
                DatabaseManager::instance().remove_offline_report(item.queue_id);
                qWarning().noquote() << QString("[EdgeReport] Dropped unroutable offline queue #%1 due to point deactivation (HTTP %2)")
                                            .arg(item.queue_id).arg(code);
                emit offline_queue_updated(DatabaseManager::instance().get_pending_offline_count());
                emit cloud_status_changed(false, "⚠️ 点位已下架或Token已注销 (本地持续自主监控)");
            } else {
                // 真正的网络中断/5xx故障，增加重试计数并退避
                DatabaseManager::instance().increment_offline_retry(item.queue_id);
                qWarning().noquote() << QString("[EdgeReport] Failed to drain offline queue #%1: %2").arg(item.queue_id).arg(err);
            }
        });
}

void EdgeReportManager::send_heartbeat(float fps, float latency_ms, int current_occupancy, const QString& backend, const QString& status) {
    current_occupancy_ = current_occupancy;
    if (is_point_deactivated_) return;
    if (cloud_base_url_.trimmed().isEmpty() || bound_point_token_.trimmed().isEmpty()) return;

    QUrl hb_url(cloud_base_url_.trimmed() + "/api/v1/edge/heartbeat");
    QNetworkRequest request(hb_url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("X-Point-Token", bound_point_token_.toUtf8());
    request.setTransferTimeout(2000);

    QJsonObject obj;
    obj["clientName"] = client_name_;
    obj["clientVersion"] = "v1.2.0";
    obj["fps"] = static_cast<double>(fps);
    obj["inferLatencyMs"] = static_cast<double>(latency_ms);
    obj["hardwareBackend"] = backend;
    obj["currentOccupancy"] = current_occupancy;
    obj["status"] = status;

    QByteArray body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    QNetworkReply* reply = nam_->post(request, body);
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}

int EdgeReportManager::get_offline_queue_count() const {
    return DatabaseManager::instance().get_pending_offline_count();
}

void EdgeReportManager::enqueue_clip_upload(const QString& clip_path, int track_id, qint64 timestamp_ms) {
    if (clip_path.trimmed().isEmpty()) return;
    QMutexLocker locker(&clip_mutex_);
    if (pending_clips_.size() >= 50) {
        // 队列超载保护: 剔除最旧待上传任务
        pending_clips_.pop_front();
    }
    pending_clips_.push_back({clip_path.trimmed(), track_id, timestamp_ms, 0});
}

void EdgeReportManager::process_clip_upload_queue() {
    if (is_point_deactivated_) {
        QMutexLocker locker(&clip_mutex_);
        pending_clips_.clear();
        return;
    }
    if (is_uploading_clip_) return;
    if (cloud_base_url_.trimmed().isEmpty() || bound_point_token_.trimmed().isEmpty()) return;

    PendingClip task;
    {
        QMutexLocker locker(&clip_mutex_);
        if (pending_clips_.empty()) return;
        task = pending_clips_.front();
    }

    QFileInfo fi(task.local_path);
    // 检查切片文件是否存在且已由 ClipRecorder 完成编码 (> 1KB)
    if (!fi.exists() || fi.size() < 1024) {
        QMutexLocker locker(&clip_mutex_);
        if (!pending_clips_.empty()) {
            pending_clips_.front().retry_count++;
            if (pending_clips_.front().retry_count > 10) {
                // 等待 20 秒仍未生成完整切片，安全放弃该任务
                pending_clips_.pop_front();
            }
        }
        return;
    }

    is_uploading_clip_ = true;

    QHttpMultiPart* multi_part = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart file_part;
    file_part.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("video/mp4"));
    file_part.setHeader(QNetworkRequest::ContentDispositionHeader,
        QVariant(QString("form-data; name=\"file\"; filename=\"%1\"").arg(fi.fileName())));

    QFile* file = new QFile(task.local_path);
    if (!file->open(QIODevice::ReadOnly)) {
        delete file;
        delete multi_part;
        is_uploading_clip_ = false;
        QMutexLocker locker(&clip_mutex_);
        if (!pending_clips_.empty()) pending_clips_.pop_front();
        return;
    }
    file_part.setBodyDevice(file);
    file->setParent(multi_part);
    multi_part->append(file_part);

    QString endpoint = QString("%1/api/v1/edge/clip/upload?trackId=%2&timestamp=%3")
        .arg(cloud_base_url_.trimmed())
        .arg(task.track_id)
        .arg(task.timestamp_ms);

    QUrl upload_url(endpoint);
    QNetworkRequest request(upload_url);
    request.setRawHeader("X-Point-Token", bound_point_token_.toUtf8());

    QNetworkReply* reply = nam_->post(request, multi_part);
    multi_part->setParent(reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply, task]() {
        reply->deleteLater();
        is_uploading_clip_ = false;

        int http_status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() == QNetworkReply::NoError && (http_status == 200 || http_status == 201)) {
            QByteArray res_bytes = reply->readAll();
            qInfo() << "[EdgeReportManager] Video clip uploaded to MinIO successfully:" << task.local_path << res_bytes;
            QMutexLocker locker(&clip_mutex_);
            if (!pending_clips_.empty() && pending_clips_.front().local_path == task.local_path) {
                pending_clips_.pop_front();
            }
        } else if (http_status == 400 || http_status == 401 || http_status == 403) {
            is_point_deactivated_ = true;
            is_online_ = false;
            QMutexLocker locker(&clip_mutex_);
            pending_clips_.clear();
            qWarning() << "[EdgeReportManager] Clip upload canceled: point token invalid/deactivated (HTTP" << http_status << "). Entering local autonomy.";
            emit cloud_status_changed(false, "⚠️ 点位已下架或Token已注销 (本地持续自主监控)");
        } else {
            qWarning() << "[EdgeReportManager] Video clip upload failed (HTTP" << http_status << "):" << reply->errorString();
            QMutexLocker locker(&clip_mutex_);
            if (!pending_clips_.empty() && pending_clips_.front().local_path == task.local_path) {
                pending_clips_.front().retry_count++;
                if (pending_clips_.front().retry_count > 5) {
                    pending_clips_.pop_front();
                }
            }
        }
    });
}

void EdgeReportManager::upload_snapshot(const QByteArray& jpeg_data) {
    if (jpeg_data.isEmpty()) return;
    if (cloud_base_url_.trimmed().isEmpty() || bound_point_token_.trimmed().isEmpty()) return;
    if (is_point_deactivated_) return;
    if (!nam_) nam_ = new QNetworkAccessManager(this);

    QUrl snap_url(cloud_base_url_.trimmed() + "/api/v1/edge/snapshot");
    QNetworkRequest request(snap_url);
    request.setRawHeader("X-Point-Token", bound_point_token_.toUtf8());

    QHttpMultiPart* multi_part = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart file_part;
    file_part.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("image/jpeg"));
    file_part.setHeader(QNetworkRequest::ContentDispositionHeader,
        QVariant("form-data; name=\"file\"; filename=\"snapshot.jpg\""));
    file_part.setBody(jpeg_data);
    multi_part->append(file_part);

    QNetworkReply* reply = nam_->post(request, multi_part);
    multi_part->setParent(reply);
    connect(reply, &QNetworkReply::finished, reply, [reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            qDebug() << "[EdgeReportManager] Live camera snapshot uploaded successfully.";
        } else {
            qWarning() << "[EdgeReportManager] Snapshot upload failed:" << reply->errorString();
        }
    });
}

