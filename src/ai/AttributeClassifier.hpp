#pragma once
#include <string>
#include <vector>
#include <memory>
#include <opencv2/core.hpp>

/**
 * @brief 目标属性识别结果
 */
struct AttributeResult {
    std::string gender = "未知";      // "男" / "女"
    std::string age_group = "未知";   // "少年(<18)" / "青年(18-35)" / "中年(36-55)" / "老年(>55)"
    float gender_confidence = 0.0f;
    float age_confidence = 0.0f;
};

/**
 * @brief 二级真实属性分类模型推理引擎 (MobileNetV3 ONNX)
 * @note  采用异步工作队列 (Worker Thread)，主视频流推流与检测完全零阻塞 (0 延迟/0 丢帧)。
 */
class AttributeClassifier {
public:
    static AttributeClassifier& instance();

    bool init(const std::string& model_path = "models/mobilenetv3_attributes.onnx");
    void stop();
    bool is_loaded() const;

    // 异步非阻塞推断：主线程裁剪目标框直接投递，后台线程池计算完毕自动回填至缓存
    void classify_async(int track_id, const cv::Mat& crop_bgr);

    // 同步推断 (可用于单体调试或精准分析)
    AttributeResult classify_sync(const cv::Mat& crop_bgr);

    // 查询目标 Track 是否已具备识别完成的属性
    bool get_attributes(int track_id, std::string& out_gender, std::string& out_age);

    // 清除指定 Track 或全量缓存
    void remove_track(int track_id);
    void clear_cache();

private:
    AttributeClassifier();
    ~AttributeClassifier();

    AttributeClassifier(const AttributeClassifier&) = delete;
    AttributeClassifier& operator=(const AttributeClassifier&) = delete;

    class Impl;
    std::unique_ptr<Impl> impl_;
};
