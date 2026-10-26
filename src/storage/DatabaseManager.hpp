#pragma once
#include "common/Types.hpp"
#include <QString>
#include <QSqlDatabase>
#include <vector>
#include <map>

// 报表检索条件
struct AnalyticsFilterCriteria {
    int64_t start_ms = 0;
    int64_t end_ms = 0;
    int direction_filter = -1; // -1: 全部, 0: 进 (ENTRY), 1: 出 (EXIT)
    QString gender_filter = ""; // "": 全部, "男", "女"
};

// 报表多维汇总指标
struct AnalyticsReportSummary {
    int total_flow = 0;
    int entry_count = 0;
    int exit_count = 0;
    int male_count = 0;
    int female_count = 0;
    int peak_hour = -1;
    int peak_hour_count = 0;
    std::string top_age_group = "暂无数据";
    int top_age_count = 0;
    std::map<int, std::pair<int, int>> hourly_data; // hour (0~23) -> {entry, exit}
};

class DatabaseManager {
public:
    static DatabaseManager& instance();

    bool init_database(const QString& db_path = "data/flowsight.db");
    bool insert_events(const std::vector<PassageEvent>& events);
    std::vector<PassageEvent> query_recent_events(int limit = 50);
    std::vector<PassageEvent> query_events_by_timerange(int64_t start_ms, int64_t end_ms, int limit = 5000);
    std::vector<PassageEvent> query_filtered_events(int direction_filter = -1, int64_t start_ms = 0, int64_t end_ms = 0, const QString& keyword = "", int limit = 500, const QString& gender_filter = "");
    std::vector<RoiConfig> load_latest_roi_configs();
    bool save_roi_config(const RoiConfig& config);
    bool save_roi_configs(const std::vector<RoiConfig>& configs);
    void get_today_counts(int& out_entry, int& out_exit);
    void prune_expired_event_clips(int64_t threshold_ms);
    bool update_event_attributes(int track_id, const std::string& gender, const std::string& age_group);
    bool update_event_vehicle_attributes(int track_id, const std::string& plate_number, const std::string& plate_color, const std::string& vehicle_type, const std::string& vehicle_color);
    QSqlDatabase get_database();

    // 报表多维综合查询与指标统计
    AnalyticsReportSummary query_analytics_summary(const AnalyticsFilterCriteria& criteria);

    // P1-6: 小时预聚合加速 (stats_hourly)
    bool sync_hourly_aggregation();
    std::vector<HourlyStat> query_hourly_stats(int64_t start_ms, int64_t end_ms);
    std::map<int, std::pair<int, int>> query_hourly_distribution(int64_t start_ms, int64_t end_ms);
    void get_demographics(int64_t start_ms, int64_t end_ms, int& out_male, int& out_female);
    void get_top_age_group(int64_t start_ms, int64_t end_ms, std::string& out_top_age, int& out_count);

    // 一键清空系统全部历史记录与预聚合数据
    bool clear_all_history_data(bool reset_roi_configs = false, int* out_deleted_events = nullptr);

    // 离线断网缓存队列 (Offline Report Queue)
    struct OfflineReportItem {
        int64_t queue_id = 0;
        QString batch_uuid;
        QString report_type;
        QByteArray payload_blob;
        int retry_count = 0;
    };

    bool enqueue_offline_report(const QString& batch_uuid, const QString& report_type, const QByteArray& payload_blob);
    std::vector<OfflineReportItem> fetch_pending_offline_reports(int limit = 5);
    bool remove_offline_report(int64_t queue_id);
    bool increment_offline_retry(int64_t queue_id);
    int get_pending_offline_count();

    // 应用程序退出时安全关闭全部线程关联的 SQLite 数据库句柄并解除锁
    void close_all_connections();

private:
    DatabaseManager() = default;
    ~DatabaseManager() = default;

    QString db_path_ = "data/flowsight.db";
};

