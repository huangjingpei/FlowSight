#pragma once
#include "common/Types.hpp"
#include <QString>
#include <QSqlDatabase>
#include <vector>
#include <map>

class DatabaseManager {
public:
    static DatabaseManager& instance();

    bool init_database(const QString& db_path = "data/flowsight.db");
    bool insert_events(const std::vector<PassageEvent>& events);
    std::vector<PassageEvent> query_recent_events(int limit = 50);
    std::vector<PassageEvent> query_events_by_timerange(int64_t start_ms, int64_t end_ms, int limit = 5000);
    std::vector<PassageEvent> query_filtered_events(int direction_filter = -1, int64_t start_ms = 0, int64_t end_ms = 0, const QString& keyword = "", int limit = 500);
    std::vector<RoiConfig> load_latest_roi_configs();
    bool save_roi_config(const RoiConfig& config);
    bool save_roi_configs(const std::vector<RoiConfig>& configs);
    void get_today_counts(int& out_entry, int& out_exit);
    void prune_expired_event_clips(int64_t threshold_ms);
    QSqlDatabase get_database();

    // P1-6: 小时预聚合加速 (stats_hourly)
    bool sync_hourly_aggregation();
    std::vector<HourlyStat> query_hourly_stats(int64_t start_ms, int64_t end_ms);
    std::map<int, std::pair<int, int>> query_hourly_distribution(int64_t start_ms, int64_t end_ms);
    void get_demographics(int64_t start_ms, int64_t end_ms, int& out_male, int& out_female);
    void get_top_age_group(int64_t start_ms, int64_t end_ms, std::string& out_top_age, int& out_count);

private:
    DatabaseManager() = default;
    ~DatabaseManager() = default;

    QString db_path_ = "data/flowsight.db";
};

