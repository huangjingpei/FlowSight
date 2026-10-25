#include "RoiCounter.hpp"
#include <opencv2/imgproc.hpp>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <iostream>
#include <cmath>
#include <algorithm>

namespace {
enum class TrackState { UNKNOWN, OUTSIDE, INSIDE };

struct TrackContext {
    TrackState state = TrackState::UNKNOWN;
    int outside_counter = 0;
    int inside_counter = 0;
    int cooldown_ticks = 0;
    int64_t last_seen_ms = 0;
    cv::Point2f start_pt{0.0f, 0.0f};
    bool has_start_pt = false;
    cv::Point2f last_pt{0.0f, 0.0f};
    bool has_last_pt = false;
    bool is_passage_locked = false; // 轨迹全生命周期防重绝对闭锁 (单人单次通行终身只触发一次)
    PassageDirection locked_dir = PassageDirection::ENTRY;
};

// 时空近邻碎片抑制记录 (防范 ByteTrack 短暂遮挡后产生新 TrackID 造成二次计数)
struct RecentPassageRecord {
    int64_t timestamp_ms = 0;
    PassageDirection direction = PassageDirection::ENTRY;
    int roi_id = 0;
    cv::Point2f foot_point{0.0f, 0.0f};
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
    float span_length = 100.0f;
};
} // namespace

class RoiCounter::Impl {
public:
    std::vector<RoiConfig> roi_configs_;
    std::unordered_map<int64_t, TrackContext> track_states_;
    std::deque<RecentPassageRecord> recent_passages_; // 最近触发事件缓冲池 (保留 3 秒)
    int clean_counter_ = 0;

    // 检查时空近邻抑制：若 1.5 秒内同一防区、同一方向在相近位置已有穿行，抑制重复碎片
    bool is_spatially_duplicate(int roi_id, PassageDirection dir, const cv::Point2f& pt, int64_t now_ms) {
        for (const auto& rec : recent_passages_) {
            if (rec.roi_id == roi_id && rec.direction == dir) {
                if (std::abs(now_ms - rec.timestamp_ms) <= 1500) {
                    float dist_sq = (pt.x - rec.foot_point.x) * (pt.x - rec.foot_point.x) +
                                    (pt.y - rec.foot_point.y) * (pt.y - rec.foot_point.y);
                    if (dist_sq <= 80.0f * 80.0f) { // 80 像素内判定为同一人由于遮挡 ID 漂移造成的重复
                        return true;
                    }
                }
            }
        }
        return false;
    }

    void process_frame(const std::vector<TrackObject>& tracks,
                       int64_t timestamp_ms,
                       int& current_occupancy,
                       std::vector<PassageEvent>& out_events,
                       int frame_w = 0,
                       int frame_h = 0) {
        current_occupancy = 0;
        out_events.clear();

        // 清理超过 3 秒的时空抑制记录
        while (!recent_passages_.empty() && (timestamp_ms - recent_passages_.front().timestamp_ms > 3000)) {
            recent_passages_.pop_front();
        }

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
                meta.span_length = std::max(60.0f, std::max(meta.max_x - meta.min_x, meta.max_y - meta.min_y));
                valid_rois.push_back(meta);
            }
        }

        // ---------------------------------------------------------------------
        // 模式 A: 全域感知模式 (未划定防区时开箱即用，通过宏观法向净位移确定进出)
        // ---------------------------------------------------------------------
        if (valid_rois.empty()) {
            current_occupancy = static_cast<int>(tracks.size());

            int w = frame_w > 0 ? frame_w : 1920;
            int h = frame_h > 0 ? frame_h : 1080;

            // 统一全域默认流向：与标准通道一致，默认 90° (向下为进，向上为出)
            float angle = 90.0f;
            bool invert = false;
            if (!roi_configs_.empty()) {
                angle = roi_configs_.front().flow_angle_deg;
                invert = roi_configs_.front().invert_direction;
            }
            cv::Point2f flow_dir = get_roi_flow_unit_vector(angle, invert);
            float d_thresh = std::max(35.0f, std::min(w, h) * 0.045f);

            for (const auto& trk : tracks) {
                int64_t key = make_track_roi_key(trk.track_id, 0);
                auto& ctx = track_states_[key];
                ctx.last_seen_ms = timestamp_ms;
                if (ctx.cooldown_ticks > 0) ctx.cooldown_ticks--;

                if (!ctx.has_start_pt) {
                    ctx.start_pt = trk.foot_point;
                    ctx.has_start_pt = true;
                }

                // 沿设定的流向法向量计算累计物理位移 disp
                float disp = (trk.foot_point.x - ctx.start_pt.x) * flow_dir.x +
                             (trk.foot_point.y - ctx.start_pt.y) * flow_dir.y;

                if (!ctx.is_passage_locked && ctx.cooldown_ticks == 0) {
                    if (disp >= d_thresh) {
                        // 顺向净位移超阈值 -> 确认为进入 (ENTRY)
                        if (!is_spatially_duplicate(0, PassageDirection::ENTRY, trk.foot_point, timestamp_ms)) {
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

                            recent_passages_.push_back({timestamp_ms, PassageDirection::ENTRY, 0, trk.foot_point});
                        }
                        ctx.is_passage_locked = true;
                        ctx.locked_dir = PassageDirection::ENTRY;
                        ctx.cooldown_ticks = 30;
                    } else if (disp <= -d_thresh) {
                        // 逆向净位移超阈值 -> 确认为离开 (EXIT)
                        if (!is_spatially_duplicate(0, PassageDirection::EXIT, trk.foot_point, timestamp_ms)) {
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

                            recent_passages_.push_back({timestamp_ms, PassageDirection::EXIT, 0, trk.foot_point});
                        }
                        ctx.is_passage_locked = true;
                        ctx.locked_dir = PassageDirection::EXIT;
                        ctx.cooldown_ticks = 30;
                    }
                }

                ctx.last_pt = trk.foot_point;
                ctx.has_last_pt = true;
            }
            return;
        }

        // ---------------------------------------------------------------------
        // 模式 B: 多边形防区模式 (宏观法向净位移主裁决 + 轨迹全生命周期防重闭锁)
        // ---------------------------------------------------------------------
        std::unordered_set<int> unique_occupants;

        for (const auto& trk : tracks) {
            for (const auto& roi_meta : valid_rois) {
                const auto& roi = roi_meta.cfg;
                const auto& poly = roi.vertices;
                float buf_dist = roi.buffer_distance;

                // 1. AABB 快速外接矩形初筛
                float effective_buf = std::max(25.0f, buf_dist);
                float aabb_pad = effective_buf + 20.0f;
                if (trk.foot_point.x < roi_meta.min_x - aabb_pad || trk.foot_point.x > roi_meta.max_x + aabb_pad ||
                    trk.foot_point.y < roi_meta.min_y - aabb_pad || trk.foot_point.y > roi_meta.max_y + aabb_pad) {

                    int64_t key = make_track_roi_key(trk.track_id, roi.roi_id);
                    auto it = track_states_.find(key);
                    if (it != track_states_.end()) {
                        it->second.last_seen_ms = timestamp_ms;
                        it->second.outside_counter++;
                        // 离开外部边界不再清空 is_passage_locked，仅复位局部中间状态
                        it->second.state = TrackState::OUTSIDE;
                    }
                    continue;
                }

                // 2. 点多边形带符号欧氏距离测试 (包含多边形内与缓冲带)
                double dist = cv::pointPolygonTest(poly, trk.foot_point, true);

                if (dist >= 0) {
                    unique_occupants.insert(trk.track_id);
                }

                int64_t key = make_track_roi_key(trk.track_id, roi.roi_id);
                auto& ctx = track_states_[key];
                ctx.last_seen_ms = timestamp_ms;
                if (ctx.cooldown_ticks > 0) ctx.cooldown_ticks--;

                // 3. 核心穿行判定 (处于多边形内部或几何缓冲带内)
                if (dist >= -effective_buf) {
                    ctx.state = TrackState::INSIDE;
                    ctx.outside_counter = 0;
                    ctx.inside_counter++;

                    // 记录进入该防区时最初可靠足底坐标
                    if (!ctx.has_start_pt) {
                        ctx.start_pt = trk.foot_point;
                        ctx.has_start_pt = true;
                    }

                    // 计算目标自进入防区以来，沿设定流向法向量的累计物理净位移 (标量)
                    float disp = (trk.foot_point.x - ctx.start_pt.x) * roi_meta.dir.x +
                                 (trk.foot_point.y - ctx.start_pt.y) * roi_meta.dir.y;

                    // 动态计算防区尺度下的稳健位移门限 (至少 35 像素，避免足底正常前后摆动干扰)
                    float d_thresh = std::max(35.0f, std::min(effective_buf * 1.8f, roi_meta.span_length * 0.25f));

                    // 仅在当前 Track 未曾触发过该防区时执行单向判定
                    if (!ctx.is_passage_locked && ctx.cooldown_ticks == 0 && ctx.inside_counter >= roi.min_confirm_frames) {
                        if (disp >= d_thresh) {
                            // 沿绿色箭头方向发生真实单向净位移 -> 确认为进入 (ENTRY)
                            if (!is_spatially_duplicate(roi.roi_id, PassageDirection::ENTRY, trk.foot_point, timestamp_ms)) {
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

                                recent_passages_.push_back({timestamp_ms, PassageDirection::ENTRY, roi.roi_id, trk.foot_point});
                            }

                            // 锁定当前 Track，杜绝在门口驻足、徘徊造成第 2 次触发！
                            ctx.is_passage_locked = true;
                            ctx.locked_dir = PassageDirection::ENTRY;
                            ctx.cooldown_ticks = 30;
                        } else if (disp <= -d_thresh) {
                            // 逆着绿色箭头方向发生真实单向净位移 -> 确认为离开 (EXIT)
                            if (!is_spatially_duplicate(roi.roi_id, PassageDirection::EXIT, trk.foot_point, timestamp_ms)) {
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

                                recent_passages_.push_back({timestamp_ms, PassageDirection::EXIT, roi.roi_id, trk.foot_point});
                            }

                            // 锁定当前 Track，杜绝在门口驻足、徘徊造成第 2 次触发！
                            ctx.is_passage_locked = true;
                            ctx.locked_dir = PassageDirection::EXIT;
                            ctx.cooldown_ticks = 30;
                        }
                    }

                    ctx.last_pt = trk.foot_point;
                    ctx.has_last_pt = true;
                } else {
                    // 4. 处于外部边界之外：仅累加离开计数，绝不解锁 is_passage_locked
                    ctx.outside_counter++;
                    ctx.state = TrackState::OUTSIDE;
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
        recent_passages_.clear();
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
