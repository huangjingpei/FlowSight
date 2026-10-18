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
    float last_h = 0.0f;
    bool has_last_h = false;
    bool passage_triggered = false; // 单次穿行防重复触发标记
};

inline int64_t make_track_roi_key(int track_id, int roi_id) {
    return (static_cast<int64_t>(track_id) << 32) | (static_cast<uint32_t>(roi_id) & 0xFFFFFFFF);
}

// 计算防区几何重心与主流动法向基准向量 (与 VideoCanvas 矢量箭头保持完全一致)
static cv::Point2f compute_flow_normal(const std::vector<cv::Point2f>& poly, const cv::Point2f& centroid, bool invert_direction) {
    cv::Point2f upper_sum(0, 0), lower_sum(0, 0);
    int upper_cnt = 0, lower_cnt = 0;
    for (const auto& pt : poly) {
        if (pt.y < centroid.y) { upper_sum += pt; upper_cnt++; }
        else { lower_sum += pt; lower_cnt++; }
    }
    cv::Point2f dir(0.0f, 1.0f);
    if (upper_cnt > 0 && lower_cnt > 0) {
        dir = (lower_sum * (1.0f / static_cast<float>(upper_cnt))) - (upper_sum * (1.0f / static_cast<float>(lower_cnt)));
    }
    float len = std::hypot(dir.x, dir.y);
    if (len > 1e-3f) dir *= (1.0f / len);
    else dir = cv::Point2f(0.0f, 1.0f);

    if (invert_direction) {
        dir = -dir;
    }
    return dir;
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
                       std::vector<PassageEvent>& out_events) {
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
                meta.dir = compute_flow_normal(r.vertices, meta.centroid, r.invert_direction);
                valid_rois.push_back(meta);
            }
        }

        if (valid_rois.empty()) {
            // 未标定特定防区时，全视口活跃目标均计入当前人数
            current_occupancy = static_cast<int>(tracks.size());
            return;
        }

        std::unordered_set<int> unique_occupants;

        for (const auto& trk : tracks) {
            for (const auto& roi_meta : valid_rois) {
                const auto& roi = roi_meta.cfg;
                const auto& poly = roi.vertices;
                float buf_dist = roi.buffer_distance;
                int min_confirm = roi.min_confirm_frames;

                // 1. AABB 快速外接矩形初筛 (提升多目标多防区检测性能)
                float aabb_pad = buf_dist + 5.0f;
                if (trk.foot_point.x < roi_meta.min_x - aabb_pad || trk.foot_point.x > roi_meta.max_x + aabb_pad ||
                    trk.foot_point.y < roi_meta.min_y - aabb_pad || trk.foot_point.y > roi_meta.max_y + aabb_pad) {
                    
                    int64_t key = make_track_roi_key(trk.track_id, roi.roi_id);
                    auto it = track_states_.find(key);
                    if (it != track_states_.end()) {
                        it->second.last_seen_ms = timestamp_ms;
                        if (++it->second.outside_counter >= min_confirm) {
                            it->second.state = TrackState::OUTSIDE;
                            it->second.passage_triggered = false;
                            it->second.has_last_h = false;
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

                // 计算目标足底相对于多边形重心的法向高度投影 h
                cv::Point2f rel_p = trk.foot_point - roi_meta.centroid;
                float h = rel_p.x * roi_meta.dir.x + rel_p.y * roi_meta.dir.y;

                // 3. 核心穿行判定 (处于多边形内部或缓冲带范围内)
                if (dist >= -buf_dist) {
                    ctx.state = TrackState::INSIDE;
                    ctx.outside_counter = 0;

                    if (!ctx.passage_triggered && ctx.has_last_h && ctx.cooldown_ticks == 0) {
                        // 从上游 (h < 0) 跨越至下游 (h >= 0): 沿基准箭头方向进入
                        if (ctx.last_h < -2.0f && h >= -2.0f) {
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
                            ctx.cooldown_ticks = 25; // 约 0.8 秒防抖
                        }
                        // 从下游 (h > 0) 跨越至上游 (h <= 0): 逆基准箭头方向离开
                        else if (ctx.last_h > 2.0f && h <= 2.0f) {
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
                            ctx.cooldown_ticks = 25;
                        }
                    }

                    ctx.last_h = h;
                    ctx.has_last_h = true;
                } else {
                    // 4. 处于外部边界之外：确认离开后复位触发标记，准备接收下一次穿行
                    if (++ctx.outside_counter >= min_confirm) {
                        ctx.state = TrackState::OUTSIDE;
                        ctx.passage_triggered = false;
                        ctx.has_last_h = false;
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
