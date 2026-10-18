#pragma once
#include "common/Types.hpp"
#include <QString>
#include <vector>

class DatabaseManager {
public:
    static DatabaseManager& instance();

    bool init_database(const QString& db_path = "data/flowsight.db");
    bool insert_events(const std::vector<PassageEvent>& events);
    std::vector<PassageEvent> query_recent_events(int limit = 50);
    std::vector<PassageEvent> query_events_by_timerange(int64_t start_ms, int64_t end_ms, int limit = 5000);
    std::vector<RoiConfig> load_latest_roi_configs();
    bool save_roi_config(const RoiConfig& config);

private:
    DatabaseManager() = default;
    ~DatabaseManager() = default;
};
