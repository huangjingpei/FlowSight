#include "OnnxDetector.hpp"
#include <onnxruntime_cxx_api.h>
#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <array>
#include <algorithm>

class OnnxDetector::Impl {
public:
    Ort::Env env_{ORT_LOGGING_LEVEL_WARNING, "YOLOv8_FlowSight"};
    Ort::SessionOptions session_options_;
    std::unique_ptr<Ort::Session> session_;
    Ort::MemoryInfo memory_info_ = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    int input_w_ = 640;
    int input_h_ = 640;
    float conf_thresh_ = 0.45f;
    float nms_thresh_ = 0.50f;
    std::string backend_name_ = "CPU";

    std::vector<std::string> input_node_names_;
    std::vector<std::string> output_node_names_;
    std::vector<const char*> input_names_ptrs_;
    std::vector<const char*> output_names_ptrs_;

    bool init(const std::string& model_path, float conf_thresh, float nms_thresh) {
        conf_thresh_ = conf_thresh;
        nms_thresh_ = nms_thresh;

        session_options_.SetIntraOpNumThreads(2);
        session_options_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        // 优先尝试启用 CUDA Provider
        try {
            OrtCUDAProviderOptions cuda_options{};
            cuda_options.device_id = 0;
            session_options_.AppendExecutionProvider_CUDA(cuda_options);
            backend_name_ = "CUDA (NVIDIA GPU)";
            std::cout << "[OnnxDetector] Successfully enabled CUDA Execution Provider." << std::endl;
        } catch (const std::exception& e) {
            backend_name_ = "CPU (Multi-Threaded)";
            std::cout << "[OnnxDetector] CUDA unavailable, falling back to CPU: " << e.what() << std::endl;
        }

        std::wstring wide_path(model_path.begin(), model_path.end());
        try {
            session_ = std::make_unique<Ort::Session>(env_, wide_path.c_str(), session_options_);
        } catch (const std::exception& e) {
            std::cerr << "[OnnxDetector] Failed to load model file: " << e.what() << std::endl;
            return false;
        }

        // 动态获取输入输出名称
        Ort::AllocatorWithDefaultOptions allocator;
        size_t num_inputs = session_->GetInputCount();
        for (size_t i = 0; i < num_inputs; ++i) {
            auto input_name = session_->GetInputNameAllocated(i, allocator);
            input_node_names_.emplace_back(input_name.get());
        }
        size_t num_outputs = session_->GetOutputCount();
        for (size_t i = 0; i < num_outputs; ++i) {
            auto output_name = session_->GetOutputNameAllocated(i, allocator);
            output_node_names_.emplace_back(output_name.get());
        }

        input_names_ptrs_.clear();
        for (const auto& name : input_node_names_) input_names_ptrs_.push_back(name.c_str());
        output_names_ptrs_.clear();
        for (const auto& name : output_node_names_) output_names_ptrs_.push_back(name.c_str());

        return true;
    }

    bool detect(const cv::Mat& bgr_frame, std::vector<Detection>& detections) {
        detections.clear();
        if (bgr_frame.empty() || !session_) return false;

        // 1. 等比 Letterbox 预处理至 640x640
        float r = std::min((float)input_w_ / bgr_frame.cols, (float)input_h_ / bgr_frame.rows);
        int unpad_w = std::round(bgr_frame.cols * r);
        int unpad_h = std::round(bgr_frame.rows * r);
        int pad_w = (input_w_ - unpad_w) / 2;
        int pad_h = (input_h_ - unpad_h) / 2;

        cv::Mat resized;
        cv::resize(bgr_frame, resized, cv::Size(unpad_w, unpad_h));
        cv::Mat input_img(input_h_, input_w_, CV_8UC3, cv::Scalar(114, 114, 114));
        resized.copyTo(input_img(cv::Rect(pad_w, pad_h, unpad_w, unpad_h)));

        // 转换 RGB，归一化并打平成 NCHW 连续内存
        cv::Mat rgb;
        cv::cvtColor(input_img, rgb, cv::COLOR_BGR2RGB);
        rgb.convertTo(rgb, CV_32FC3, 1.0f / 255.0f);

        std::vector<float> input_tensor_values(1 * 3 * input_h_ * input_w_);
        int channel_size = input_h_ * input_w_;
        std::vector<cv::Mat> channels(3);
        for (int i = 0; i < 3; ++i) {
            channels[i] = cv::Mat(input_h_, input_w_, CV_32FC1, input_tensor_values.data() + i * channel_size);
        }
        cv::split(rgb, channels);

        std::array<int64_t, 4> input_shape = {1, 3, input_h_, input_w_};
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info_, input_tensor_values.data(), input_tensor_values.size(),
            input_shape.data(), input_shape.size()
        );

        // 2. 执行模型推理
        auto output_tensors = session_->Run(
            Ort::RunOptions{nullptr},
            input_names_ptrs_.data(), &input_tensor, 1,
            output_names_ptrs_.data(), output_names_ptrs_.size()
        );

        if (output_tensors.empty()) return false;

        // 3. 解析输出张量 (YOLOv8 形状: [1, 84, 8400])
        float* raw_output = output_tensors[0].GetTensorMutableData<float>();
        auto output_shape = output_tensors[0].GetTensorTypeAndShapeInfo().GetShape();
        if (output_shape.size() < 3) return false;

        int dimensions = static_cast<int>(output_shape[1]);  // 84
        int num_proposals = static_cast<int>(output_shape[2]); // 8400

        std::vector<cv::Rect> boxes;
        std::vector<float> confidences;

        for (int i = 0; i < num_proposals; ++i) {
            // YOLOv8 class 0 is person
            float person_score = raw_output[4 * num_proposals + i];
            if (person_score >= conf_thresh_) {
                float cx = raw_output[0 * num_proposals + i];
                float cy = raw_output[1 * num_proposals + i];
                float w = raw_output[2 * num_proposals + i];
                float h = raw_output[3 * num_proposals + i];

                // 还原到原始帧尺寸
                float orig_x = (cx - pad_w) / r - (w / (2.0f * r));
                float orig_y = (cy - pad_h) / r - (h / (2.0f * r));
                float orig_w = w / r;
                float orig_h = h / r;

                boxes.emplace_back(cv::Rect(std::round(orig_x), std::round(orig_y), std::round(orig_w), std::round(orig_h)));
                confidences.push_back(person_score);
            }
        }

        // 4. OpenCV NMS 消除冗余重叠框
        std::vector<int> indices;
        cv::dnn::NMSBoxes(boxes, confidences, conf_thresh_, nms_thresh_, indices);

        for (int idx : indices) {
            Detection det;
            det.bbox = cv::Rect2f(boxes[idx]);
            det.confidence = confidences[idx];
            det.class_id = 0;
            detections.push_back(det);
        }

        return true;
    }
};

OnnxDetector::OnnxDetector() : impl_(std::make_unique<Impl>()) {}
OnnxDetector::~OnnxDetector() = default;

bool OnnxDetector::init(const std::string& model_path, float conf_thresh, float nms_thresh) {
    return impl_->init(model_path, conf_thresh, nms_thresh);
}

bool OnnxDetector::detect(const cv::Mat& bgr_frame, std::vector<Detection>& detections) {
    return impl_->detect(bgr_frame, detections);
}

std::string OnnxDetector::get_backend_name() const {
    return impl_->backend_name_;
}
