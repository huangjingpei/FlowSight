#include "RoiCounter.hpp"
#include <opencv2/imgproc.hpp>
#include <unordered_map>
#include <iostream>

namespace {
enum class TrackState { UNKNOWN, OUTSIDE, INSIDE };

struct TrackContext {
    TrackState state = TrackState::UNKNOWN;
    int inside_counter = 0;
    int outside_counter = 0;
    int cooldown_ticks = 0;
};
} // namespace

class RoiCounter::Impl {
public:
    std::shared_ptr<const RoiConfig> roi_config_;
    std::unordered_map<int, TrackContext> track_states_;

    void process_frame(const std::vector<TrackObject>& tracks,
                       int64_t timestamp_ms,
                       int& current_occupancy,
                       std::vector<PassageEvent>& out_events) {
        current_occupancy = 0;
        out_events.clear();

        if (!roi_config_ || !roi_config_->is_valid || roi_config_->vertices.size() < 3) {
            return;
        }

        const auto& poly = roi_config_->vertices;
        float buf_dist = roi_config_->buffer_distance;
        int min_confirm = roi_config_->min_confirm_frames;

        for (const auto& trk : tracks) {
            // 点多边形带符号欧氏距离测试
            double dist = cv::pointPolygonTest(poly, trk.foot_point, true);

            // 在物理边界内即计入当前人数
            if (dist >= 0) {
                current_occupancy++;
            }

            auto& ctx = track_states_[trk.track_id];
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
                        ev.direction = PassageDirection::ENTRY;
                        ev.roi_version = roi_config_->version;
                        ev.confidence = trk.score;
                        ev.source_id = roi_config_->source_id;
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
                        ev.direction = PassageDirection::EXIT;
                        ev.roi_version = roi_config_->version;
                        ev.confidence = trk.score;
                        ev.source_id = roi_config_->source_id;
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

    void reset() {
        track_states_.clear();
    }
};

RoiCounter::RoiCounter() : impl_(std::make_unique<Impl>()) {}
RoiCounter::~RoiCounter() = default;

void RoiCounter::set_roi_config(std::shared_ptr<const RoiConfig> config) {
    impl_->roi_config_ = config;
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
