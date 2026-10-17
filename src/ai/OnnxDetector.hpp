#pragma once
#include "IDetector.hpp"
#include <memory>

class OnnxDetector : public IDetector {
public:
    OnnxDetector();
    ~OnnxDetector() override;

    bool init(const std::string& model_path, float conf_thresh = 0.45f, float nms_thresh = 0.50f) override;
    bool detect(const cv::Mat& bgr_frame, std::vector<Detection>& detections) override;
    std::string get_backend_name() const override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
