#pragma once
#include "common/Types.hpp"
#include <string>
#include <vector>
#include <opencv2/core.hpp>

class IDetector {
public:
    virtual ~IDetector() = default;

    // 初始化模型文件并设置检测及 NMS 阈值
    virtual bool init(const std::string& model_path, float conf_thresh = 0.45f, float nms_thresh = 0.50f) = 0;

    // 执行单帧目标检测
    virtual bool detect(const cv::Mat& bgr_frame, std::vector<Detection>& detections) = 0;

    // 获取当前推理后端名称
    virtual std::string get_backend_name() const = 0;

    // 动态切换业务检测模式 ("PEDESTRIAN" 行人 / "VEHICLE" 车辆)
    virtual void set_biz_type(const std::string& biz_type) {}
};
