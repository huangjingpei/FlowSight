#include "WebhookPusher.hpp"
#include "storage/ConfigManager.hpp"
#include <QJsonObject>
#include <QJsonDocument>
#include <QDateTime>
#include <QElapsedTimer>
#include <QDebug>

WebhookPusher& WebhookPusher::instance() {
    static WebhookPusher inst;
    return inst;
}

WebhookPusher::WebhookPusher(QObject* parent) : QObject(parent) {
    nam_ = new QNetworkAccessManager(this);
}

void WebhookPusher::init(QObject* parent) {
    if (parent && this->parent() != parent) {
        setParent(parent);
    }
}

void WebhookPusher::push_event(const PassageEvent& event) {
    auto& cfg = ConfigManager::instance();
    if (!cfg.get_webhook_enabled()) {
        return;
    }

    QUrl url(cfg.get_webhook_url().trimmed());
    if (!url.isValid() || url.scheme().isEmpty()) {
        qWarning() << "[Webhook] Invalid URL configured:" << cfg.get_webhook_url();
        return;
    }

    QJsonObject obj;
    obj["event_type"] = "PASSAGE";
    obj["direction"] = (event.direction == PassageDirection::ENTRY) ? "ENTRY" : "EXIT";
    obj["track_id"] = event.track_id;
    obj["timestamp"] = static_cast<qint64>(event.timestamp_ms);
    obj["datetime"] = QDateTime::fromMSecsSinceEpoch(event.timestamp_ms).toString(Qt::ISODateWithMs);
    obj["confidence"] = static_cast<double>(event.confidence);
    obj["source_id"] = QString::fromStdString(event.source_id);
    obj["clip_path"] = QString::fromStdString(event.clip_path);
    obj["roi_version"] = event.roi_version;

    QJsonObject attrs;
    attrs["gender"] = QString::fromStdString(event.gender);
    attrs["age_group"] = QString::fromStdString(event.age_group);
    obj["attributes"] = attrs;

    QJsonDocument doc(obj);
    QByteArray body = doc.toJson(QJsonDocument::Compact);

    send_json_post(url, cfg.get_webhook_token(), cfg.get_webhook_timeout_ms(), body,
        [this, event](bool success, int code, const QString& err, qint64 latency) {
            if (success) {
                total_pushed_++;
                qInfo().noquote() << QString("[Webhook] Pushed Track %1 (%2) successfully in %3ms (HTTP %4)")
                                        .arg(event.track_id)
                                        .arg(event.direction == PassageDirection::ENTRY ? "ENTRY" : "EXIT")
                                        .arg(latency)
                                        .arg(code);
            } else {
                total_failed_++;
                qWarning().noquote() << QString("[Webhook] Push failed for Track %1: %2 (code %3)")
                                            .arg(event.track_id).arg(err).arg(code);
            }
        });
}

void WebhookPusher::push_surge_alert(bool is_surged, int current_occupancy, int threshold) {
    auto& cfg = ConfigManager::instance();
    if (!cfg.get_webhook_enabled()) {
        return;
    }

    QUrl url(cfg.get_webhook_url().trimmed());
    if (!url.isValid() || url.scheme().isEmpty()) {
        return;
    }

    QJsonObject obj;
    obj["event_type"] = "CROWD_SURGE";
    obj["is_surged"] = is_surged;
    obj["current_occupancy"] = current_occupancy;
    obj["threshold"] = threshold;
    obj["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    obj["datetime"] = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    obj["message"] = is_surged ? QString::fromUtf8("区域客流超载预警触发！") : QString::fromUtf8("区域客流超载预警已恢复常态");

    QJsonDocument doc(obj);
    QByteArray body = doc.toJson(QJsonDocument::Compact);

    send_json_post(url, cfg.get_webhook_token(), cfg.get_webhook_timeout_ms(), body,
        [is_surged, current_occupancy, threshold](bool success, int code, const QString& err, qint64 latency) {
            if (success) {
                qInfo().noquote() << QString("[Webhook] Pushed crowd surge alert (surged=%1, occ=%2/%3) in %4ms")
                                        .arg(is_surged).arg(current_occupancy).arg(threshold).arg(latency);
            } else {
                qWarning().noquote() << QString("[Webhook] Failed to push surge alert: %1 (code %2)").arg(err).arg(code);
            }
        });
}

void WebhookPusher::test_ping(const QString& target_url, const QString& auth_token, int timeout_ms, PingCallback callback) {
    QUrl url(target_url.trimmed());
    if (!url.isValid() || url.scheme().isEmpty()) {
        if (callback) {
            callback(false, 0, QString::fromUtf8("URL 格式无效，必须包含 http:// 或 https://"), 0);
        }
        return;
    }

    QJsonObject obj;
    obj["event_type"] = "PING";
    obj["client"] = "FlowSight";
    obj["version"] = "1.2.0";
    obj["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    obj["datetime"] = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    obj["message"] = "FlowSight Webhook connectivity test";

    QJsonDocument doc(obj);
    QByteArray body = doc.toJson(QJsonDocument::Compact);

    send_json_post(url, auth_token, timeout_ms, body, callback);
}

void WebhookPusher::send_json_post(const QUrl& url, const QString& token, int timeout_ms, const QByteArray& json_data,
                                  std::function<void(bool success, int code, const QString& err, qint64 latency)> callback) {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!token.trimmed().isEmpty()) {
        request.setRawHeader("Authorization", "Bearer " + token.trimmed().toUtf8());
    }

    int t_ms = timeout_ms > 0 ? timeout_ms : 3000;
    request.setTransferTimeout(t_ms);

    auto* timer = new QElapsedTimer();
    timer->start();

    QNetworkReply* reply = nam_->post(request, json_data);

    connect(reply, &QNetworkReply::finished, this, [this, reply, timer, callback]() {
        qint64 latency = timer->elapsed();
        delete timer;

        int status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        bool success = (reply->error() == QNetworkReply::NoError);
        QString msg;

        if (success) {
            msg = QString("HTTP %1 OK").arg(status_code);
        } else {
            msg = reply->errorString();
            if (status_code > 0) {
                msg += QString(" (HTTP %1)").arg(status_code);
            }
        }

        emit push_completed(success, status_code, msg, latency);

        if (callback) {
            callback(success, status_code, msg, latency);
        }

        reply->deleteLater();
    });
}
