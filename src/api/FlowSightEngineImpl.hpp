#pragma once

#include "flowsight/FlowSightAPI.h"
#include "ai/OnnxDetector.hpp"
#include "tracking/ByteTrackWrapper.hpp"
#include "analytics/RoiCounter.hpp"
#include "common/Types.hpp"

#include <thread>
#include <mutex>
#include <atomic>
#include <string>
#include <vector>
#include <memory>
#include <chrono>

namespace flowsight {

class FlowSightEngineImpl : public IFlowSightEngine {
public:
    FlowSightEngineImpl();
    ~FlowSightEngineImpl() override;

    bool initialize(const std::string& model_path, float conf_thresh = 0.45f, int device_id = 0) override;
    void register_event_callback(FlowEventCallback callback, void* user_context = nullptr) override;
    void register_stats_callback(SystemStatsCallback callback, void* user_context = nullptr) override;

    bool set_detection_region(const PolygonRegion& region) override;
    bool start_stream(const std::string& video_uri) override;
    void stop_stream() override;

    int32_t add_channel(const ChannelConfig& config) override;
    bool remove_channel(int32_t channel_id) override;
    bool start_channel(int32_t channel_id) override;
    void stop_channel(int32_t channel_id) override;
    bool set_channel_region(int32_t channel_id, const PolygonRegion& region) override;
    std::vector<ChannelStatus> get_all_channels_status() const override;

    // 纯 C 兼容导出辅助方法
    void register_c_event_callback(C_FlowEventCallback callback, void* user_data);
    void register_c_stats_callback(C_SystemStatsCallback callback, void* user_data);
    bool set_detection_region_c(int32_t roi_id, const char* roi_name, const C_Point2D* points, int num_points);
    bool get_current_stats_c(C_SystemStats* out_stats);

private:
    void worker_loop(const std::string video_uri);

    std::unique_ptr<OnnxDetector> detector_;
    std::unique_ptr<ByteTrackWrapper> tracker_;
    std::unique_ptr<RoiCounter> roi_counter_;

    mutable std::mutex config_mutex_;
    std::vector<RoiConfig> roi_configs_;
    int64_t next_event_id_{1};

    FlowEventCallback cpp_event_cb_{nullptr};
    void* cpp_event_user_{nullptr};
    SystemStatsCallback cpp_stats_cb_{nullptr};
    void* cpp_stats_user_{nullptr};

    C_FlowEventCallback c_event_cb_{nullptr};
    void* c_event_user_{nullptr};
    C_SystemStatsCallback c_stats_cb_{nullptr};
    void* c_stats_user_{nullptr};

    std::atomic<bool> initialized_{false};
    std::atomic<bool> running_{false};
    std::thread worker_thread_;

    mutable std::mutex stats_mutex_;
    int total_entry_{0};
    int total_exit_{0};
    int current_occupancy_{0};
    float last_infer_ms_{0.0f};
    float last_fps_{0.0f};
};

} // namespace flowsight
