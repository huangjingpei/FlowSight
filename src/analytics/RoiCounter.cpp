#include "RoiCounter.hpp"
#include <opencv2/imgproc.hpp>
#include <unordered_map>
#include <unordered_set>
#include <iostream>

namespace {
enum class TrackState { UNKNOWN, OUTSIDE, INSIDE };

struct TrackContext {
    TrackState state = TrackState::UNKNOWN;
    int inside_counter = 0;
    int outside_counter = 0;
    int cooldown_ticks = 0;
    int64_t last_seen_ms = 0;
};

inline int64_t make_track_roi_key(int track_id, int roi_id) {
    return (static_cast<int64_t>(track_id) << 32) | (static_cast<uint32_t>(roi_id) & 0xFFFFFFFF);
}
} // namespace

class RoiCounter::Impl {
public:
    std::vector<RoiConfig> roi_configs_;
    std::unordered_map<int64_t, TrackContext> track_states_;
    int clean_counter_ = 0;

    void process_frame(const std::vector<TrackObject>& tracks,
                       int64_t timestamp_ms,
                       int& current_occupancy,
                       std::vector<PassageEvent>& out_events) {
        current_occupancy = 0;
        out_events.clear();

        std::vector<RoiConfig> valid_rois;
        for (const auto& r : roi_configs_) {
            if (r.is_valid && r.vertices.size() >= 3) {
                valid_rois.push_back(r);
            }
        }

        if (valid_rois.empty()) {
            // 未标定特定防区时，全视口活跃目标均计入当前人数
            current_occupancy = static_cast<int>(tracks.size());
            return;
        }

        std::unordered_set<int> unique_occupants;

        for (const auto& trk : tracks) {
            for (const auto& roi : valid_rois) {
                const auto& poly = roi.vertices;
                float buf_dist = roi.buffer_distance;
                int min_confirm = roi.min_confirm_frames;

                // 点多边形带符号欧氏距离测试
                double dist = cv::pointPolygonTest(poly, trk.foot_point, true);

                if (dist >= 0) {
                    unique_occupants.insert(trk.track_id);
                }

                int64_t key = make_track_roi_key(trk.track_id, roi.roi_id);
                auto& ctx = track_states_[key];
                ctx.last_seen_ms = timestamp_ms;
                if (ctx.cooldown_ticks > 0) ctx.cooldown_ticks--;

                // 初始状态判定
                if (ctx.state == TrackState::UNKNOWN) {
                    if (dist >= buf_dist) ctx.state = TrackState::INSIDE;
                    else if (dist <= -buf_dist) ctx.state = TrackState::OUTSIDE;
                    continue;
                }

                // 当前在外部：穿入核心区判定
                if (ctx.state == TrackState::OUTSIDE) {
                    if (dist >= buf_dist) {
                        ctx.inside_counter++;
                        if (ctx.inside_counter >= min_confirm && ctx.cooldown_ticks == 0) {
                            ctx.state = TrackState::INSIDE;
                            ctx.inside_counter = 0;
                            ctx.cooldown_ticks = 30; // 冷却约 1 秒防反向误抖

                            PassageEvent ev;
                            ev.timestamp_ms = timestamp_ms;
                            ev.track_id = trk.track_id;
                            ev.direction = roi.invert_direction ? PassageDirection::EXIT : PassageDirection::ENTRY;
                            ev.roi_version = roi.version;
                            ev.roi_id = roi.roi_id;
                            ev.roi_name = roi.name;
                            ev.confidence = trk.score;
                            ev.source_id = roi.source_id;
                            ev.gender = trk.gender;
                            ev.age_group = trk.age_group;
                            out_events.push_back(ev);
                        }
                    } else {
                        ctx.inside_counter = 0;
                    }
                }
                // 当前在内部：穿出核心区判定
                else if (ctx.state == TrackState::INSIDE) {
                    if (dist <= -buf_dist) {
                        ctx.outside_counter++;
                        if (ctx.outside_counter >= min_confirm && ctx.cooldown_ticks == 0) {
                            ctx.state = TrackState::OUTSIDE;
                            ctx.outside_counter = 0;
                            ctx.cooldown_ticks = 30;

                            PassageEvent ev;
                            ev.timestamp_ms = timestamp_ms;
                            ev.track_id = trk.track_id;
                            ev.direction = roi.invert_direction ? PassageDirection::ENTRY : PassageDirection::EXIT;
                            ev.roi_version = roi.version;
                            ev.roi_id = roi.roi_id;
                            ev.roi_name = roi.name;
                            ev.confidence = trk.score;
                            ev.source_id = roi.source_id;
                            ev.gender = trk.gender;
                            ev.age_group = trk.age_group;
                            out_events.push_back(ev);
                        }
                    } else {
                        ctx.outside_counter = 0;
                    }
                }
            }
        }

        current_occupancy = static_cast<int>(unique_occupants.size());

        // 定期清理长时间未出现的轨迹状态，防止内存持续膨胀 (>10s)
        if (++clean_counter_ >= 100) {
            clean_counter_ = 0;
            for (auto it = track_states_.begin(); it != track_states_.end(); ) {
                if (timestamp_ms - it->second.last_seen_ms > 10000 || timestamp_ms < it->second.last_seen_ms) {
                    it = track_states_.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    void reset() {
        track_states_.clear();
    }
};

RoiCounter::RoiCounter() : impl_(std::make_unique<Impl>()) {}
RoiCounter::~RoiCounter() = default;

void RoiCounter::set_roi_config(std::shared_ptr<const RoiConfig> config) {
    if (config) {
        set_roi_configs({*config});
    } else {
        set_roi_configs({});
    }
}

void RoiCounter::set_roi_configs(const std::vector<RoiConfig>& configs) {
    impl_->roi_configs_ = configs;
    impl_->reset();
}

void RoiCounter::process_frame(const std::vector<TrackObject>& tracks,
                              int64_t timestamp_ms,
                              int& current_occupancy,
                              std::vector<PassageEvent>& out_events) {
    impl_->process_frame(tracks, timestamp_ms, current_occupancy, out_events);
}

void RoiCounter::reset() {
    impl_->reset();
}
