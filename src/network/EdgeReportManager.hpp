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
#include <deque>
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

    // 异步校准内存聚合窗口中事件的 AI 属性 (防止二级分类完成时事件已进入批量缓冲)
    void update_event_attributes(int track_id, const std::string& gender, const std::string& age_group);

    // 手动/定时触发聚合包刷新上报
    void flush_batch();

    // 3. 硬件与性能遥测心跳上报
    void send_heartbeat(float fps, float latency_ms, int current_occupancy, const QString& backend = "CPU");

    // 4. 手动发送测试聚合上报包 (用于设置界面联调验证)
    void send_test_batch(std::function<void(bool success, const QString& msg, qint64 latency_ms)> callback);

    // 5. 动态拉取云端点位配置 (RTSP 摄像头流、点位名称、分辨率)
    using ConfigCallback = std::function<void(bool success, const QString& msg, const QJsonObject& point_config)>;
    void fetch_cloud_config(ConfigCallback callback);

    // 获取离线积压队列数量
    int get_offline_queue_count() const;

    // 当前是否在线与点位有效性状态
    bool is_cloud_online() const { return is_online_; }
    bool is_point_deactivated() const { return is_point_deactivated_; }

    QString get_cloud_base_url() const { return cloud_base_url_; }
    qint64 get_bound_point_id() const { return bound_point_id_; }
    QString get_bound_point_name() const { return bound_point_name_; }
    QString get_bound_point_token() const { return bound_point_token_; }
    QString get_client_name() const { return client_name_; }

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

    // MinIO 短视频切片异步上云引擎
    struct PendingClip {
        QString local_path;
        int track_id = -1;
        qint64 timestamp_ms = 0;
        int retry_count = 0;
    };
    void enqueue_clip_upload(const QString& clip_path, int track_id, qint64 timestamp_ms);
    void process_clip_upload_queue();

    QNetworkAccessManager* nam_ = nullptr;
    QTimer* batch_timer_ = nullptr;
    QTimer* watchdog_timer_ = nullptr;
    QTimer* clip_upload_timer_ = nullptr;

    std::deque<PendingClip> pending_clips_;
    QMutex clip_mutex_;
    bool is_uploading_clip_ = false;

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
    bool is_point_deactivated_ = false;
    bool is_draining_ = false;

    // 本地持久化配置缓存
    QString cloud_base_url_;
    qint64 bound_point_id_ = 0;
    QString bound_point_name_;
    QString bound_point_token_;
    QString client_name_ = "FlowSight-Node-01";
    int window_seconds_ = 10;
};
