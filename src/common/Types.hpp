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

// 进出事件类型
enum class PassageDirection {
    ENTRY = 1,
    EXIT = 2
};

// 通行事实明细（含短视频切片路径）
struct PassageEvent {
    int64_t timestamp_ms = 0;
    int track_id = -1;
    PassageDirection direction = PassageDirection::ENTRY;
    int roi_version = 0;
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
