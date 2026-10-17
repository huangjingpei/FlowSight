#include "ByteTrackWrapper.hpp"
#include <opencv2/imgproc.hpp>
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>

namespace {

enum class STrackState { New = 0, Tracked, Lost, Removed };

struct STrack {
    int track_id = 0;
    cv::Rect2f bbox;
    float score = 0.0f;
    STrackState state = STrackState::New;
    int tracklet_len = 0;
    int lost_frame_count = 0;
    cv::Point2f velocity{0.0f, 0.0f};

    void predict() {
        // 简单匀速运动学预测
        bbox.x += velocity.x;
        bbox.y += velocity.y;
    }

    void update(const Detection& det) {
        cv::Point2f old_center(bbox.x + bbox.width * 0.5f, bbox.y + bbox.height * 0.5f);
        cv::Point2f new_center(det.bbox.x + det.bbox.width * 0.5f, det.bbox.y + det.bbox.height * 0.5f);
        velocity = (new_center - old_center) * 0.4f + velocity * 0.6f;

        bbox = det.bbox;
        score = det.confidence;
        state = STrackState::Tracked;
        tracklet_len++;
        lost_frame_count = 0;
    }

    void mark_lost() {
        state = STrackState::Lost;
        lost_frame_count++;
    }
};

float calculate_iou(const cv::Rect2f& bb_test, const cv::Rect2f& bb_gt) {
    float in = (bb_test & bb_gt).area();
    float un = bb_test.area() + bb_gt.area() - in;
    if (un <= 0.0f) return 0.0f;
    return in / un;
}

} // namespace

class ByteTrackWrapper::Impl {
public:
    int frame_rate_ = 30;
    int track_buffer_ = 30;
    int next_id_ = 1;
    std::vector<STrack> tracked_stracks_;
    std::vector<STrack> lost_stracks_;

    Impl(int frame_rate, int track_buffer)
        : frame_rate_(frame_rate), track_buffer_(track_buffer) {}

    std::vector<TrackObject> update(const std::vector<Detection>& detections) {
        // 1. 分离高置信度与低置信度检测框 (ByteTrack 核心原理)
        std::vector<Detection> det_first;
        std::vector<Detection> det_second;

        for (const auto& det : detections) {
            if (det.confidence >= 0.5f) {
                det_first.push_back(det);
            } else if (det.confidence >= 0.2f) {
                det_second.push_back(det);
            }
        }

        // 2. 预测当前跟踪目标位置
        for (auto& st : tracked_stracks_) st.predict();
        for (auto& st : lost_stracks_) st.predict();

        std::vector<STrack> pool_tracks;
        for (const auto& st : tracked_stracks_) pool_tracks.push_back(st);
        for (const auto& st : lost_stracks_) pool_tracks.push_back(st);

        // 3. 第一轮关联：高分检测框与活跃/丢失轨迹做 IoU 匹配
        std::vector<bool> det_matched(det_first.size(), false);
        std::vector<bool> track_matched(pool_tracks.size(), false);
        std::vector<STrack> updated_tracks;

        for (size_t d = 0; d < det_first.size(); ++d) {
            float best_iou = 0.25f; // IoU 阈值
            int best_t = -1;
            for (size_t t = 0; t < pool_tracks.size(); ++t) {
                if (track_matched[t]) continue;
                float iou = calculate_iou(det_first[d].bbox, pool_tracks[t].bbox);
                if (iou > best_iou) {
                    best_iou = iou;
                    best_t = static_cast<int>(t);
                }
            }

            if (best_t != -1) {
                track_matched[best_t] = true;
                det_matched[d] = true;
                pool_tracks[best_t].update(det_first[d]);
                updated_tracks.push_back(pool_tracks[best_t]);
            }
        }

        // 4. 第二轮关联：剩余未匹配轨迹与低分检测框匹配（恢复被遮挡目标）
        std::vector<STrack> remain_tracks;
        for (size_t t = 0; t < pool_tracks.size(); ++t) {
            if (!track_matched[t]) {
                remain_tracks.push_back(pool_tracks[t]);
            }
        }

        for (const auto& det : det_second) {
            float best_iou = 0.35f;
            int best_idx = -1;
            for (size_t t = 0; t < remain_tracks.size(); ++t) {
                float iou = calculate_iou(det.bbox, remain_tracks[t].bbox);
                if (iou > best_iou) {
                    best_iou = iou;
                    best_idx = static_cast<int>(t);
                }
            }
            if (best_idx != -1) {
                remain_tracks[best_idx].update(det);
                updated_tracks.push_back(remain_tracks[best_idx]);
                remain_tracks.erase(remain_tracks.begin() + best_idx);
            }
        }

        // 5. 对剩余未匹配的高分框创建新轨迹
        for (size_t d = 0; d < det_first.size(); ++d) {
            if (!det_matched[d]) {
                STrack new_track;
                new_track.track_id = next_id_++;
                new_track.bbox = det_first[d].bbox;
                new_track.score = det_first[d].confidence;
                new_track.state = STrackState::Tracked;
                new_track.tracklet_len = 1;
                new_track.lost_frame_count = 0;
                updated_tracks.push_back(new_track);
            }
        }

        // 6. 更新 tracked 与 lost 列表，清理超出存活周期的轨迹
        tracked_stracks_.clear();
        lost_stracks_.clear();

        for (const auto& st : updated_tracks) {
            tracked_stracks_.push_back(st);
        }

        for (auto& st : remain_tracks) {
            st.mark_lost();
            if (st.lost_frame_count < track_buffer_) {
                lost_stracks_.push_back(st);
            }
        }

        // 7. 构筑输出 TrackObject 列表并计算自适应足底代表点
        std::vector<TrackObject> output_tracks;
        for (const auto& st : tracked_stracks_) {
            TrackObject obj;
            obj.track_id = st.track_id;
            obj.bbox = st.bbox;
            obj.score = st.score;
            obj.track_len = st.tracklet_len;

            // 足底接触点 (x1+x2)/2, y2
            obj.foot_point = cv::Point2f(st.bbox.x + st.bbox.width * 0.5f, st.bbox.y + st.bbox.height);

            // 预留属性展示（可随机或通过规则打标测试）
            obj.gender = (st.track_id % 2 == 0) ? "男" : "女";
            obj.age_group = "20-30";

            output_tracks.push_back(obj);
        }

        return output_tracks;
    }

    void reset() {
        tracked_stracks_.clear();
        lost_stracks_.clear();
        next_id_ = 1;
    }
};

ByteTrackWrapper::ByteTrackWrapper(int frame_rate, int track_buffer)
    : impl_(std::make_unique<Impl>(frame_rate, track_buffer)) {}

ByteTrackWrapper::~ByteTrackWrapper() = default;

std::vector<TrackObject> ByteTrackWrapper::update(const std::vector<Detection>& detections) {
    return impl_->update(detections);
}

void ByteTrackWrapper::reset() {
    impl_->reset();
}
