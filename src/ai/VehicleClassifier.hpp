#pragma once
#include <string>
#include <vector>
#include <memory>
#include <opencv2/core.hpp>
#include "common/Types.hpp"

/**
 * @brief 智能边缘车流与车牌识别 (LPR) 分析引擎
 * @note  采用异步 Worker 线程池架构，主推理流水线 0 阻塞，支持多帧轨迹时序加权融合
 */
class VehicleClassifier {
public:
    static VehicleClassifier& instance();

    bool init(const std::string& model_path = "");
    void stop();
    bool is_loaded() const;

    // 异步非阻塞推断：主视频线程裁剪目标框直接投递，后台计算完毕自动回填至缓存
    void classify_async(int track_id, int class_id, const cv::Mat& crop_bgr);

    // 同步推断 (可用于单体调试或精准分析)
    VehicleInfo classify_sync(int track_id, int class_id, const cv::Mat& crop_bgr);

    // 查询目标 Track 是否已具备识别完成的车辆与车牌属性
    bool get_vehicle_info(int track_id, VehicleInfo& out_info);

    // 清除指定 Track 或全量缓存
    void remove_track(int track_id);
    void clear_cache();

private:
    VehicleClassifier();
    ~VehicleClassifier();

    VehicleClassifier(const VehicleClassifier&) = delete;
    VehicleClassifier& operator=(const VehicleClassifier&) = delete;

    class Impl;
    std::unique_ptr<Impl> impl_;
};
