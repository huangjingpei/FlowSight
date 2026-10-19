#include "RoiCounter.hpp"
#include <opencv2/imgproc.hpp>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <cmath>
#include <algorithm>

namespace {
enum class TrackState { UNKNOWN, OUTSIDE, INSIDE };

struct TrackContext {
    TrackState state = TrackState::UNKNOWN;
    int outside_counter = 0;
    int cooldown_ticks = 0;
    int64_t last_seen_ms = 0;
    cv::Point2f start_pt{0.0f, 0.0f};
    bool has_start_pt = false;
    cv::Point2f last_pt{0.0f, 0.0f};
    bool has_last_pt = false;
    float last_side = 0.0f;
    bool has_last_side = false;
    bool passage_triggered = false; // 单次穿行防重复触发标记
    PassageDirection triggered_dir = PassageDirection::ENTRY;
};

inline int64_t make_track_roi_key(int track_id, int roi_id) {
    return (static_cast<int64_t>(track_id) << 32) | (static_cast<uint32_t>(roi_id) & 0xFFFFFFFF);
}

// 计算防区进方向法向基准向量 (依据 flow_angle_deg 与 invert_direction，与 VideoCanvas 矢量箭头保持完全一致)
static cv::Point2f compute_flow_normal(const RoiConfig& roi) {
    return get_roi_flow_unit_vector(roi.flow_angle_deg, roi.invert_direction);
}

struct RoiMeta {
    RoiConfig cfg;
    cv::Point2f centroid{0, 0};
    cv::Point2f dir{0, 1};
    float min_x = 0;
    float max_x = 0;
    float min_y = 0;
    float max_y = 0;
};
} // namespace

class RoiCounter::Impl {
public:
    std::vector<RoiConfig> roi_configs_;
    std::unordered_map<int64_t, TrackContext> track_states_;
    int clean_counter_ = 0;

    void process_frame(const std::vector<TrackObject>& tracks,
                       int64_t timestamp_ms,
                       int& current_occupancy,
                       std::vector<PassageEvent>& out_events,
                       int frame_w = 0,
                       int frame_h = 0) {
        current_occupancy = 0;
        out_events.clear();

        std::vector<RoiMeta> valid_rois;
        for (const auto& r : roi_configs_) {
            if (r.is_valid && r.vertices.size() >= 3) {
                RoiMeta meta;
                meta.cfg = r;
                meta.min_x = r.vertices[0].x; meta.max_x = r.vertices[0].x;
                meta.min_y = r.vertices[0].y; meta.max_y = r.vertices[0].y;
                for (const auto& v : r.vertices) {
                    meta.centroid += v;
                    meta.min_x = std::min(meta.min_x, v.x);
                    meta.max_x = std::max(meta.max_x, v.x);
                    meta.min_y = std::min(meta.min_y, v.y);
                    meta.max_y = std::max(meta.max_y, v.y);
                }
                meta.centroid *= (1.0f / static_cast<float>(r.vertices.size()));
                meta.dir = compute_flow_normal(r);
                valid_rois.push_back(meta);
            }
        }

        // ---------------------------------------------------------------------
        // 模式 A: 全域感知模式 (未划定防区时开箱即用，通过画面中央虚拟穿越基线自动计数)
        // ---------------------------------------------------------------------
        if (valid_rois.empty()) {
            current_occupancy = static_cast<int>(tracks.size());

            int w = frame_w > 0 ? frame_w : 1920;
            int h = frame_h > 0 ? frame_h : 1080;
            cv::Point2f center_pt(w * 0.5f, h * 0.5f);

            // 优先继承当前配置的流向角度，若未设置则默认 270° (向上纵深进入)
            float angle = 270.0f;
            bool invert = false;
            if (!roi_configs_.empty()) {
                angle = roi_configs_.front().flow_angle_deg;
                invert = roi_configs_.front().invert_direction;
            }
            cv::Point2f flow_dir = get_roi_flow_unit_vector(angle, invert);
            float d_thresh = std::max(30.0f, std::min(w, h) * 0.05f);

            for (const auto& trk : tracks) {
                int64_t key = make_track_roi_key(trk.track_id, 0);
                auto& ctx = track_states_[key];
                ctx.last_seen_ms = timestamp_ms;
                if (ctx.cooldown_ticks > 0) ctx.cooldown_ticks--;

                if (!ctx.has_start_pt) {
                    ctx.start_pt = trk.foot_point;
                    ctx.has_start_pt = true;
                }

                float side = (trk.foot_point.x - center_pt.x) * flow_dir.x + (trk.foot_point.y - center_pt.y) * flow_dir.y;
                float disp = (trk.foot_point.x - ctx.start_pt.x) * flow_dir.x + (trk.foot_point.y - ctx.start_pt.y) * flow_dir.y;

                if (!ctx.passage_triggered && ctx.cooldown_ticks == 0 && ctx.has_last_side) {
                    bool forward = (disp >= d_thresh) || (ctx.last_side < -3.0f && side >= -3.0f);
                    bool backward = (disp <= -d_thresh) || (ctx.last_side > 3.0f && side <= 3.0f);

                    if (forward) {
                        PassageEvent ev;
                        ev.timestamp_ms = timestamp_ms;
                        ev.track_id = trk.track_id;
                        ev.direction = PassageDirection::ENTRY;
                        ev.roi_id = 0;
                        ev.roi_name = "全域感知";
                        ev.confidence = trk.score;
                        ev.foot_x = trk.foot_point.x;
                        ev.foot_y = trk.foot_point.y;
                        ev.gender = trk.gender;
                        ev.age_group = trk.age_group;
                        out_events.push_back(ev);
                        ctx.passage_triggered = true;
                        ctx.triggered_dir = PassageDirection::ENTRY;
                        ctx.cooldown_ticks = 25;
                    } else if (backward) {
                        PassageEvent ev;
                        ev.timestamp_ms = timestamp_ms;
                        ev.track_id = trk.track_id;
                        ev.direction = PassageDirection::EXIT;
                        ev.roi_id = 0;
                        ev.roi_name = "全域感知";
                        ev.confidence = trk.score;
                        ev.foot_x = trk.foot_point.x;
                        ev.foot_y = trk.foot_point.y;
                        ev.gender = trk.gender;
                        ev.age_group = trk.age_group;
                        out_events.push_back(ev);
                        ctx.passage_triggered = true;
                        ctx.triggered_dir = PassageDirection::EXIT;
                        ctx.cooldown_ticks = 25;
                    }
                }

                ctx.last_side = side;
                ctx.has_last_side = true;
                ctx.last_pt = trk.foot_point;
                ctx.has_last_pt = true;
            }
            return;
        }

        // ---------------------------------------------------------------------
        // 模式 B: 多边形防区模式 (双重融合判定: 轨迹累计位移向量 + 几何中线穿越)
        // ---------------------------------------------------------------------
        std::unordered_set<int> unique_occupants;

        for (const auto& trk : tracks) {
            for (const auto& roi_meta : valid_rois) {
                const auto& roi = roi_meta.cfg;
                const auto& poly = roi.vertices;
                float buf_dist = roi.buffer_distance;
                int min_confirm = roi.min_confirm_frames;

                // 1. AABB 快速外接矩形初筛 (提升多目标多防区检测性能)
                float aabb_pad = buf_dist + 10.0f;
                if (trk.foot_point.x < roi_meta.min_x - aabb_pad || trk.foot_point.x > roi_meta.max_x + aabb_pad ||
                    trk.foot_point.y < roi_meta.min_y - aabb_pad || trk.foot_point.y > roi_meta.max_y + aabb_pad) {

                    int64_t key = make_track_roi_key(trk.track_id, roi.roi_id);
                    auto it = track_states_.find(key);
                    if (it != track_states_.end()) {
                        it->second.last_seen_ms = timestamp_ms;
                        if (++it->second.outside_counter >= min_confirm) {
                            it->second.state = TrackState::OUTSIDE;
                            it->second.passage_triggered = false;
                            it->second.has_start_pt = false;
                            it->second.has_last_side = false;
                            it->second.has_last_pt = false;
                        }
                    }
                    continue;
                }

                // 2. 点多边形带符号欧氏距离测试
                double dist = cv::pointPolygonTest(poly, trk.foot_point, true);

                if (dist >= 0) {
                    unique_occupants.insert(trk.track_id);
                }

                int64_t key = make_track_roi_key(trk.track_id, roi.roi_id);
                auto& ctx = track_states_[key];
                ctx.last_seen_ms = timestamp_ms;
                if (ctx.cooldown_ticks > 0) ctx.cooldown_ticks--;

                // 3. 核心穿行判定 (处于多边形内部或几何缓冲带范围内)
                if (dist >= -buf_dist) {
                    ctx.state = TrackState::INSIDE;
                    ctx.outside_counter = 0;

                    if (!ctx.has_start_pt) {
                        ctx.start_pt = trk.foot_point;
                        ctx.has_start_pt = true;
                    }

                    // 计算目标足底相对于多边形重心的法向中线距离 side
                    cv::Point2f rel_p = trk.foot_point - roi_meta.centroid;
                    float side = rel_p.x * roi_meta.dir.x + rel_p.y * roi_meta.dir.y;

                    // 计算自进入该防区以来的累计有效流向位移 disp
                    float disp = (trk.foot_point.x - ctx.start_pt.x) * roi_meta.dir.x +
                                 (trk.foot_point.y - ctx.start_pt.y) * roi_meta.dir.y;

                    // 动态计算该防区尺度下的位移触发门限 (保证灵敏性与抗抖动的最佳平衡)
                    float d_thresh = std::max(22.0f, buf_dist * 2.2f);

                    if (!ctx.passage_triggered && ctx.cooldown_ticks == 0) {
                        // 判定条件 1: 沿流向发生实质性单向净位移
                        bool forward_disp = (disp >= d_thresh);
                        bool backward_disp = (disp <= -d_thresh);

                        // 判定条件 2: 跨越防区几何法向中线 (从上游 side < -2 跨越至下游 side >= -2)
                        bool forward_cross = (ctx.has_last_side && ctx.last_side < -2.0f && side >= -2.0f);
                        bool backward_cross = (ctx.has_last_side && ctx.last_side > 2.0f && side <= 2.0f);

                        if (forward_disp || forward_cross) {
                            PassageEvent ev;
                            ev.timestamp_ms = timestamp_ms;
                            ev.track_id = trk.track_id;
                            ev.direction = PassageDirection::ENTRY;
                            ev.roi_version = roi.version;
                            ev.roi_id = roi.roi_id;
                            ev.roi_name = roi.name;
                            ev.confidence = trk.score;
                            ev.foot_x = trk.foot_point.x;
                            ev.foot_y = trk.foot_point.y;
                            ev.source_id = roi.source_id;
                            ev.gender = trk.gender;
                            ev.age_group = trk.age_group;
                            out_events.push_back(ev);

                            ctx.passage_triggered = true;
                            ctx.triggered_dir = PassageDirection::ENTRY;
                            ctx.cooldown_ticks = 25; // 约 0.8 秒防抖
                        } else if (backward_disp || backward_cross) {
                            PassageEvent ev;
                            ev.timestamp_ms = timestamp_ms;
                            ev.track_id = trk.track_id;
                            ev.direction = PassageDirection::EXIT;
                            ev.roi_version = roi.version;
                            ev.roi_id = roi.roi_id;
                            ev.roi_name = roi.name;
                            ev.confidence = trk.score;
                            ev.foot_x = trk.foot_point.x;
                            ev.foot_y = trk.foot_point.y;
                            ev.source_id = roi.source_id;
                            ev.gender = trk.gender;
                            ev.age_group = trk.age_group;
                            out_events.push_back(ev);

                            ctx.passage_triggered = true;
                            ctx.triggered_dir = PassageDirection::EXIT;
                            ctx.cooldown_ticks = 25;
                        }
                    }

                    ctx.last_side = side;
                    ctx.has_last_side = true;
                    ctx.last_pt = trk.foot_point;
                    ctx.has_last_pt = true;
                } else {
                    // 4. 处于外部边界之外：确认离开后复位触发标记，准备接收下一次穿行
                    if (++ctx.outside_counter >= min_confirm) {
                        ctx.state = TrackState::OUTSIDE;
                        ctx.passage_triggered = false;
                        ctx.has_start_pt = false;
                        ctx.has_last_side = false;
                        ctx.has_last_pt = false;
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
                              std::vector<PassageEvent>& out_events,
                              int frame_w,
                              int frame_h) {
    impl_->process_frame(tracks, timestamp_ms, current_occupancy, out_events, frame_w, frame_h);
}

void RoiCounter::reset() {
    impl_->reset();
}
