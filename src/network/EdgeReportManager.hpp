#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <QMutex>
#include <QJsonObject>
#include <QJsonArray>
#include <functional>
#include <vector>
#include "common/Types.hpp"

/**
 * 边端核心智能上报与离线容灾管理器 (EdgeReportManager)
 * 职责:
 * 1. 统一手机号+密码登录并绑定云端点位资产
 * 2. 时间窗口批量聚合 (避免高频逐帧上报，10~30s 聚合窗口)
 * 3. GZIP/Deflate 二进制高效压缩传输 (报文体积节流 80%+)
 * 4. 本地 SQLite 离线队列表自愈与断网自动重传 (Watchdog Drainer)
 */
class EdgeReportManager : public QObject {
    Q_OBJECT
public:
    static EdgeReportManager& instance();

    void init(QObject* parent = nullptr);

    // 1. 统一手机号 + 密码登录
    using LoginCallback = std::function<void(bool success, const QString& msg, const QJsonArray& points)>;
    void login_with_credentials(const QString& base_url, const QString& phone, const QString& password, LoginCallback callback);

    // 绑定选中的云端点位
    void bind_point(const QString& base_url, qint64 point_id, const QString& point_name, const QString& point_token, const QString& client_name = "");

    // 2. 累加通行事件到当前聚合窗口
    void record_passage(const PassageEvent& event);

    // 手动/定时触发聚合包刷新上报
    void flush_batch();

    // 3. 硬件与性能遥测心跳上报
    void send_heartbeat(float fps, float latency_ms, int current_occupancy, const QString& backend = "CPU");

    // 获取离线积压队列数量
    int get_offline_queue_count() const;

    // 当前是否在线
    bool is_cloud_online() const { return is_online_; }

signals:
    void cloud_status_changed(bool is_online, const QString& status_text);
    void batch_reported(bool success, int event_count, qint64 latency_ms);
    void offline_queue_updated(int pending_count);

private slots:
    void on_batch_timer_timeout();
    void on_watchdog_timer_timeout();

private:
    explicit EdgeReportManager(QObject* parent = nullptr);
    ~EdgeReportManager() override = default;

    // 二进制高压缩封装
    QByteArray compress_data(const QByteArray& raw_json);

    // 发送带有 GZIP 标识的 HTTP POST
    void post_compressed_json(const QUrl& url, const QString& token, const QByteArray& compressed_blob,
                              const QString& batch_uuid, std::function<void(bool success, int code, const QString& err)> callback);

    // 离线队列排空引擎
    void drain_offline_queue();

    QNetworkAccessManager* nam_ = nullptr;
    QTimer* batch_timer_ = nullptr;
    QTimer* watchdog_timer_ = nullptr;

    QMutex mutex_;
    // 内存聚合池
    int in_increment_ = 0;
    int out_increment_ = 0;
    int current_occupancy_ = 0;
    int male_count_ = 0;
    int female_count_ = 0;
    int child_count_ = 0;
    int adult_count_ = 0;
    int senior_count_ = 0;
    qint64 window_start_ts_ = 0;
    std::vector<PassageEvent> batch_events_;

    bool is_online_ = false;
    bool is_draining_ = false;

    // 本地持久化配置缓存
    QString cloud_base_url_;
    qint64 bound_point_id_ = 0;
    QString bound_point_name_;
    QString bound_point_token_;
    QString client_name_ = "FlowSight-Node-01";
    int window_seconds_ = 10;
};
