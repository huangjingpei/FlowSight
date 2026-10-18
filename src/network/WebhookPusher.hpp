#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <functional>
#include "common/Types.hpp"

class WebhookPusher : public QObject {
    Q_OBJECT
public:
    static WebhookPusher& instance();

    void init(QObject* parent = nullptr);

    // 异步推送客流进出通行事件 (非阻塞，零延迟)
    void push_event(const PassageEvent& event);

    // 异步推送区域客流超载/拥挤度风控告警
    void push_surge_alert(bool is_surged, int current_occupancy, int threshold);

    // 连通性测试 Ping 回调类型: success, http_status, error_or_body, latency_ms
    using PingCallback = std::function<void(bool success, int http_status, const QString& msg, qint64 latency_ms)>;
    void test_ping(const QString& target_url, const QString& auth_token, int timeout_ms, PingCallback callback);

    qint64 get_total_pushed() const { return total_pushed_; }
    qint64 get_total_failed() const { return total_failed_; }

signals:
    void push_completed(bool success, int http_status, const QString& message, qint64 latency_ms);

private:
    explicit WebhookPusher(QObject* parent = nullptr);
    ~WebhookPusher() override = default;

    void send_json_post(const QUrl& url, const QString& token, int timeout_ms, const QByteArray& json_data,
                        std::function<void(bool success, int code, const QString& err, qint64 latency)> callback = nullptr);

    QNetworkAccessManager* nam_ = nullptr;
    qint64 total_pushed_ = 0;
    qint64 total_failed_ = 0;
};
