#include "FlowSightEngineImpl.hpp"
#include <opencv2/opencv.hpp>
#include <iostream>

namespace flowsight {

FlowSightEngineImpl::FlowSightEngineImpl() = default;

FlowSightEngineImpl::~FlowSightEngineImpl() {
    stop_stream();
}

bool FlowSightEngineImpl::initialize(const std::string& model_path, float conf_thresh, int device_id) {
    std::lock_guard<std::mutex> lock(config_mutex_);

    detector_ = std::make_unique<OnnxDetector>();
    if (!detector_->init(model_path, conf_thresh, 0.50f)) {
        return false;
    }

    tracker_ = std::make_unique<ByteTrackWrapper>(30, 30);
    roi_counter_ = std::make_unique<RoiCounter>();

    initialized_ = true;
    return true;
}

void FlowSightEngineImpl::register_event_callback(FlowEventCallback callback, void* user_context) {
    cpp_event_cb_ = std::move(callback);
    cpp_event_user_ = user_context;
}

void FlowSightEngineImpl::register_stats_callback(SystemStatsCallback callback, void* user_context) {
    cpp_stats_cb_ = std::move(callback);
    cpp_stats_user_ = user_context;
}

void FlowSightEngineImpl::register_c_event_callback(C_FlowEventCallback callback, void* user_data) {
    c_event_cb_ = callback;
    c_event_user_ = user_data;
}

void FlowSightEngineImpl::register_c_stats_callback(C_SystemStatsCallback callback, void* user_data) {
    c_stats_cb_ = callback;
    c_stats_user_ = user_data;
}

bool FlowSightEngineImpl::set_detection_region(const PolygonRegion& region) {
    std::lock_guard<std::mutex> lock(config_mutex_);

    RoiConfig cfg;
    cfg.roi_id = (region.channel_id >= 0) ? (region.channel_id + 1) : 1;
    cfg.name = region.region_name.empty() ? (region.region_id.empty() ? "默认防区" : region.region_id) : region.region_name;
    cfg.color_hex = "#58a6ff";
    cfg.buffer_distance = 15.0f;
    cfg.min_confirm_frames = 4;
    cfg.invert_direction = false;

    for (const auto& pt : region.vertex_points) {
        cfg.vertices.emplace_back(pt.x, pt.y);
    }
    cfg.is_valid = (cfg.vertices.size() >= 3);

    roi_configs_ = {cfg};
    if (roi_counter_) {
        roi_counter_->set_roi_configs(roi_configs_);
    }
    return true;
}

bool FlowSightEngineImpl::set_detection_region_c(int32_t roi_id, const char* roi_name, const C_Point2D* points, int num_points) {
    std::lock_guard<std::mutex> lock(config_mutex_);

    RoiConfig cfg;
    cfg.roi_id = roi_id;
    cfg.name = (roi_name && roi_name[0] != '\0') ? roi_name : ("防区 #" + std::to_string(roi_id));
    cfg.color_hex = "#58a6ff";
    cfg.buffer_distance = 15.0f;
    cfg.min_confirm_frames = 4;
    cfg.invert_direction = false;

    if (points && num_points >= 3) {
        for (int i = 0; i < num_points; ++i) {
            cfg.vertices.emplace_back(points[i].x, points[i].y);
        }
        cfg.is_valid = true;
    } else {
        cfg.is_valid = false;
    }

    roi_configs_ = {cfg};
    if (roi_counter_) {
        roi_counter_->set_roi_configs(roi_configs_);
    }
    return true;
}

bool FlowSightEngineImpl::start_stream(const std::string& video_uri) {
    if (!initialized_) {
        return false;
    }

    if (running_) {
        stop_stream();
    }

    running_ = true;
    worker_thread_ = std::thread(&FlowSightEngineImpl::worker_loop, this, video_uri);
    return true;
}

void FlowSightEngineImpl::stop_stream() {
    running_ = false;
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

int32_t FlowSightEngineImpl::add_channel(const ChannelConfig& config) {
    set_detection_region(config.region);
    return config.channel_id;
}

bool FlowSightEngineImpl::remove_channel(int32_t channel_id) {
    (void)channel_id;
    return true;
}

bool FlowSightEngineImpl::start_channel(int32_t channel_id) {
    (void)channel_id;
    return true;
}

void FlowSightEngineImpl::stop_channel(int32_t channel_id) {
    (void)channel_id;
}

bool FlowSightEngineImpl::set_channel_region(int32_t channel_id, const PolygonRegion& region) {
    (void)channel_id;
    return set_detection_region(region);
}

std::vector<ChannelStatus> FlowSightEngineImpl::get_all_channels_status() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    ChannelStatus status;
    status.channel_id = 0;
    status.camera_id = "CAM_01";
    status.is_active = running_.load();
    status.current_fps = last_fps_;
    status.today_entry = total_entry_;
    status.today_exit = total_exit_;
    return {status};
}

bool FlowSightEngineImpl::get_current_stats_c(C_SystemStats* out_stats) {
    if (!out_stats) return false;
    std::lock_guard<std::mutex> lock(stats_mutex_);
    out_stats->current_occupancy = current_occupancy_;
    out_stats->today_entry = total_entry_;
    out_stats->today_exit = total_exit_;
    out_stats->today_total = total_entry_ + total_exit_;
    out_stats->infer_ms = last_infer_ms_;
    out_stats->fps = last_fps_;
    return true;
}

void FlowSightEngineImpl::worker_loop(const std::string video_uri) {
    cv::VideoCapture cap(video_uri);
    if (!cap.isOpened()) {
        running_ = false;
        return;
    }

    double cap_fps = cap.get(cv::CAP_PROP_FPS);
    if (cap_fps <= 5.0 || cap_fps > 120.0) {
        cap_fps = 30.0;
    }
    int frame_pacing_ms = static_cast<int>(1000.0 / cap_fps);

    cv::Mat frame;
    int frame_idx = 0;
    int fps_count = 0;
    float current_fps = static_cast<float>(cap_fps);
    auto last_fps_time = std::chrono::steady_clock::now();

    while (running_) {
        auto loop_start = std::chrono::steady_clock::now();

        if (!cap.read(frame) || frame.empty()) {
            // 本地视频自动循环播放，保持持续在线感知
            cap.set(cv::CAP_PROP_POS_FRAMES, 0);
            if (!cap.read(frame) || frame.empty()) {
                break;
            }
        }

        // 1. YOLOv8n 前向推理
        std::vector<Detection> detections;
        auto t_det0 = std::chrono::steady_clock::now();
        detector_->detect(frame, detections);
        auto t_det1 = std::chrono::steady_clock::now();
        float infer_ms = std::chrono::duration<float, std::milli>(t_det1 - t_det0).count();

        // 2. ByteTrack 轨迹关联
        auto tracks = tracker_->update(detections);

        // 3. Multi-ROI 迟滞带去重与通行判定
        int occupancy = 0;
        std::vector<PassageEvent> events;
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

        roi_counter_->process_frame(tracks, now_ms, occupancy, events);

        // 4. 更新内部快照指标
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            current_occupancy_ = occupancy;
            last_infer_ms_ = infer_ms;
            for (const auto& ev : events) {
                if (ev.direction == PassageDirection::ENTRY) total_entry_++;
                else if (ev.direction == PassageDirection::EXIT) total_exit_++;
            }
        }

        // 5. 分发通行事件回调
        for (const auto& ev : events) {
            int64_t eid = next_event_id_++;

            // C++ std::function 回调
            if (cpp_event_cb_) {
                FlowEventDTO dto;
                dto.channel_id = 0;
                dto.camera_id = "CAM_01";
                dto.event_id = eid;
                dto.timestamp_ms = ev.timestamp_ms;
                dto.track_id = ev.track_id;
                dto.direction = (ev.direction == PassageDirection::ENTRY) ? Direction::ENTRY : Direction::EXIT;
                dto.region_id = ev.roi_name;
                dto.confidence = ev.confidence;
                dto.foot_location = Point2D{0.0f, 0.0f};
                dto.clip_file_path = ev.clip_path;
                cpp_event_cb_(dto, cpp_event_user_);
            }

            // 扁平纯 C 函数指针回调
            if (c_event_cb_) {
                C_FlowEvent cev;
                cev.channel_id = 0;
                cev.event_id = eid;
                cev.timestamp_ms = ev.timestamp_ms;
                cev.track_id = ev.track_id;
                cev.direction = (ev.direction == PassageDirection::ENTRY) ? 1 : 2;
                cev.roi_id = ev.roi_id;
                cev.roi_name = ev.roi_name.c_str();
                cev.confidence = ev.confidence;
                cev.foot_x = 0.0f;
                cev.foot_y = 0.0f;
                cev.clip_file_path = ev.clip_path.c_str();
                cev.gender = ev.gender.c_str();
                cev.age_group = ev.age_group.c_str();
                c_event_cb_(&cev, c_event_user_);
            }
        }

        // 6. 测算 FPS 与定时分发指标回调
        fps_count++;
        auto now_steady = std::chrono::steady_clock::now();
        float elapsed_fps = std::chrono::duration<float>(now_steady - last_fps_time).count();
        if (elapsed_fps >= 1.0f) {
            current_fps = static_cast<float>(fps_count) / elapsed_fps;
            fps_count = 0;
            last_fps_time = now_steady;
            std::lock_guard<std::mutex> lock(stats_mutex_);
            last_fps_ = current_fps;
        }

        if (frame_idx++ % 15 == 0) {
            if (cpp_stats_cb_) {
                SystemStatsDTO s;
                s.channel_id = 0;
                s.camera_id = "CAM_01";
                {
                    std::lock_guard<std::mutex> lock(stats_mutex_);
                    s.current_occupancy = current_occupancy_;
                    s.total_entry_count = total_entry_;
                    s.total_exit_count = total_exit_;
                    s.inference_latency_ms = last_infer_ms_;
                    s.pipeline_fps = last_fps_;
                }
                cpp_stats_cb_(s, cpp_stats_user_);
            }

            if (c_stats_cb_) {
                C_SystemStats cs;
                get_current_stats_c(&cs);
                c_stats_cb_(&cs, c_stats_user_);
            }
        }

        // 7. 帧率步调控制
        auto loop_elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - loop_start
        ).count();
        if (loop_elapsed_ms < frame_pacing_ms) {
            std::this_thread::sleep_for(std::chrono::milliseconds(frame_pacing_ms - loop_elapsed_ms));
        }
    }

    cap.release();
}

} // namespace flowsight

// ============================================================================
// C-ABI 导出符号实现 (extern "C")
// ============================================================================

extern "C" {

FLOWSIGHT_API flowsight::IFlowSightEngine* CreateFlowSightEngine() {
    return new flowsight::FlowSightEngineImpl();
}

FLOWSIGHT_API void DestroyFlowSightEngine(flowsight::IFlowSightEngine* engine) {
    delete engine;
}

FLOWSIGHT_API void* FlowSight_Create() {
    return new flowsight::FlowSightEngineImpl();
}

FLOWSIGHT_API void FlowSight_Destroy(void* handle) {
    if (handle) {
        auto* engine = static_cast<flowsight::FlowSightEngineImpl*>(handle);
        delete engine;
    }
}

FLOWSIGHT_API int FlowSight_Initialize(void* handle, const char* model_path, float conf_thresh, int device_id) {
    if (!handle || !model_path) return -1;
    auto* engine = static_cast<flowsight::FlowSightEngineImpl*>(handle);
    return engine->initialize(model_path, conf_thresh, device_id) ? 0 : -2;
}

FLOWSIGHT_API int FlowSight_StartStream(void* handle, const char* video_uri) {
    if (!handle || !video_uri) return -1;
    auto* engine = static_cast<flowsight::FlowSightEngineImpl*>(handle);
    return engine->start_stream(video_uri) ? 0 : -2;
}

FLOWSIGHT_API void FlowSight_StopStream(void* handle) {
    if (handle) {
        auto* engine = static_cast<flowsight::FlowSightEngineImpl*>(handle);
        engine->stop_stream();
    }
}

FLOWSIGHT_API int FlowSight_SetDetectionRegion(void* handle, int32_t roi_id, const char* roi_name, const C_Point2D* points, int num_points) {
    if (!handle || !points || num_points < 3) return -1;
    auto* engine = static_cast<flowsight::FlowSightEngineImpl*>(handle);
    return engine->set_detection_region_c(roi_id, roi_name, points, num_points) ? 0 : -2;
}

FLOWSIGHT_API void FlowSight_RegisterEventCallback(void* handle, C_FlowEventCallback callback, void* user_data) {
    if (handle) {
        auto* engine = static_cast<flowsight::FlowSightEngineImpl*>(handle);
        engine->register_c_event_callback(callback, user_data);
    }
}

FLOWSIGHT_API void FlowSight_RegisterStatsCallback(void* handle, C_SystemStatsCallback callback, void* user_data) {
    if (handle) {
        auto* engine = static_cast<flowsight::FlowSightEngineImpl*>(handle);
        engine->register_c_stats_callback(callback, user_data);
    }
}

FLOWSIGHT_API int FlowSight_GetStats(void* handle, C_SystemStats* out_stats) {
    if (!handle || !out_stats) return -1;
    auto* engine = static_cast<flowsight::FlowSightEngineImpl*>(handle);
    return engine->get_current_stats_c(out_stats) ? 0 : -2;
}

} // extern "C"
