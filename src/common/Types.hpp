#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <opencv2/core.hpp>

// 目标检测框结构
struct Detection {
    cv::Rect2f bbox;        // 像素坐标 [x, y, w, h]
    float confidence;      // 置信度 [0.0, 1.0]
    int class_id;          // 目标类别 (0: person)
};

// 目标跟踪轨迹结构（含预留属性字段）
struct TrackObject {
    int track_id = -1;     // 唯一跟踪 ID
    cv::Rect2f bbox;       // 滤波后目标框
    cv::Point2f foot_point;// 足底接触代表点
    float score = 0.0f;    // 跟踪置信度
    int track_len = 0;     // 存活帧数
    
    // 预留基础客流属性
    std::string gender = "未知";     // "男" / "女" / "未知"
    std::string age_group = "未知";  // "青年(20-30)" / "中年(30-50)" / "未知"
};

// 任意 N 边形 ROI 配置
struct RoiConfig {
    int roi_id = 1;                    // 防区唯一编号 (1, 2, ...)
    std::string name = "A门主通道";    // 防区业务名称 (如 "A门主通道", "B门侧通道")
    std::string color_hex = "#58a6ff"; // 标定渲染强调色 (如 #58a6ff, #f1e05a, #bc8cff)
    int version = 0;
    std::string source_id = "default";
    int ref_width = 1920;
    int ref_height = 1080;
    std::vector<cv::Point2f> vertices; // 任意 N 边形原始像素顶点 (N >= 3)
    float buffer_distance = 20.0f;     // 几何缓冲带距离 (px)
    int min_confirm_frames = 4;        // 状态确认帧数
    bool invert_direction = false;     // 是否反转进出判定朝向
    bool is_valid = false;
};

inline RoiConfig make_default_corridor_roi(int width = 1920, int height = 1080, int roi_id = 1, const std::string& name = "A门主通道", const std::string& color = "#58a6ff") {
    RoiConfig cfg;
    cfg.roi_id = roi_id;
    cfg.name = name;
    cfg.color_hex = color;
    cfg.version = 1;
    cfg.source_id = "default";
    cfg.ref_width = width > 0 ? width : 1920;
    cfg.ref_height = height > 0 ? height : 1080;
    float w = static_cast<float>(cfg.ref_width);
    float h = static_cast<float>(cfg.ref_height);
    // 经典地面梯形通道防区 (覆盖主通道地面)
    cfg.vertices = {
        cv::Point2f(0.12f * w, 0.38f * h),
        cv::Point2f(0.88f * w, 0.38f * h),
        cv::Point2f(0.95f * w, 0.95f * h),
        cv::Point2f(0.05f * w, 0.95f * h)
    };
    cfg.buffer_distance = 10.0f;
    cfg.min_confirm_frames = 2;
    cfg.invert_direction = false;
    cfg.is_valid = true;
    return cfg;
}

inline RoiConfig adapt_roi_to_resolution(const RoiConfig& src, int target_w, int target_h) {
    if (src.vertices.empty() || target_w <= 0 || target_h <= 0) return src;
    int src_w = src.ref_width > 0 ? src.ref_width : target_w;
    int src_h = src.ref_height > 0 ? src.ref_height : target_h;
    if (src_w == target_w && src_h == target_h) return src;
    RoiConfig adapted = src;
    float sx = static_cast<float>(target_w) / static_cast<float>(src_w);
    float sy = static_cast<float>(target_h) / static_cast<float>(src_h);
    for (auto& v : adapted.vertices) {
        v.x *= sx;
        v.y *= sy;
    }
    adapted.ref_width = target_w;
    adapted.ref_height = target_h;
    // 随分辨率动态缩放几何缓冲带半径，确保在 432p/720p/1080p/4K 均灵敏准确
    float scale_avg = (sx + sy) * 0.5f;
    adapted.buffer_distance = std::clamp(src.buffer_distance * scale_avg, 6.0f, 25.0f);
    return adapted;
}

inline std::vector<RoiConfig> adapt_rois_to_resolution(const std::vector<RoiConfig>& src_rois, int target_w, int target_h) {
    std::vector<RoiConfig> adapted;
    adapted.reserve(src_rois.size());
    for (const auto& roi : src_rois) {
        adapted.push_back(adapt_roi_to_resolution(roi, target_w, target_h));
    }
    return adapted;
}

// 进出事件类型
enum class PassageDirection {
    ENTRY = 1,
    EXIT = 2
};

// 通行事实明细（含短视频切片路径与所属防区）
struct PassageEvent {
    int64_t timestamp_ms = 0;
    int track_id = -1;
    PassageDirection direction = PassageDirection::ENTRY;
    int roi_version = 0;
    int roi_id = 1;                     // 所属防区编号
    std::string roi_name = "A门主通道"; // 所属防区名称
    float confidence = 0.0f;
    std::string source_id;
    std::string clip_path;   // 关联的短视频切片路径 (如 "clips/clip_12.mp4")
    std::string gender = "未知";
    std::string age_group = "未知";
};

// 实时统计快照
struct StatisticsSnapshot {
    int current_occupancy = 0;
    int today_entry = 0;
    int today_exit = 0;
    int today_total = 0;
    float fps = 0.0f;
    float infer_ms = 0.0f;
    std::string backend_name = "CPU";
};
