#include "OnnxDetector.hpp"
#include <onnxruntime_cxx_api.h>
#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <array>
#include <algorithm>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <thread>

static bool is_nvidia_driver_available() {
#if defined(_WIN32)
    HMODULE hMod = LoadLibraryA("nvcuda.dll");
    if (hMod) {
        FreeLibrary(hMod);
        return true;
    }
    return false;
#else
    return false;
#endif
}

static bool is_cuda_provider_available() {
    try {
        auto providers = Ort::GetAvailableProviders();
        for (const auto& p : providers) {
            if (p == "CUDAExecutionProvider") {
                return true;
            }
        }
    } catch (...) {}
    return false;
}

class OnnxDetector::Impl {
public:
    Ort::Env env_{ORT_LOGGING_LEVEL_FATAL, "YOLOv8_FlowSight"};
    std::unique_ptr<Ort::Session> session_;
    Ort::MemoryInfo memory_info_ = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    int input_w_ = 640;
    int input_h_ = 640;
    float conf_thresh_ = 0.45f;
    float nms_thresh_ = 0.50f;
    std::string backend_name_ = "CPU (多线程高性能优化)";

    std::vector<std::string> input_node_names_;
    std::vector<std::string> output_node_names_;
    std::vector<const char*> input_names_ptrs_;
    std::vector<const char*> output_names_ptrs_;

    // 预分配常驻热路径内存池 (消除每一帧 15MB 堆内存分配与垃圾回收停顿)
    cv::Mat canvas_mat_;
    cv::Mat resized_mat_;
    cv::Mat blob_mat_;
    std::array<int64_t, 4> input_shape_{1, 3, 640, 640};

    bool init(const std::string& model_path, float conf_thresh, float nms_thresh) {
        conf_thresh_ = conf_thresh;
        nms_thresh_ = nms_thresh;
        input_shape_ = {1, 3, input_h_, input_w_};
        canvas_mat_ = cv::Mat(input_h_, input_w_, CV_8UC3, cv::Scalar(114, 114, 114));

        std::wstring wide_path(model_path.begin(), model_path.end());
        bool cuda_initialized = false;

        // 1. 默认优先尝试启用 NVIDIA GPU (CUDA)
        bool has_nvidia_gpu = is_nvidia_driver_available();
        bool has_cuda_provider = is_cuda_provider_available();

        if (has_nvidia_gpu && has_cuda_provider) {
            try {
                Ort::SessionOptions cuda_options;
                cuda_options.SetIntraOpNumThreads(2);
                cuda_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

                OrtCUDAProviderOptions cuda_opts{};
                cuda_opts.device_id = 0;
                cuda_options.AppendExecutionProvider_CUDA(cuda_opts);

                session_ = std::make_unique<Ort::Session>(env_, wide_path.c_str(), cuda_options);
                backend_name_ = "NVIDIA GPU (CUDA)";
                cuda_initialized = true;
                std::cout << "[OnnxDetector] 成功启用 NVIDIA 独立显卡硬件加速 (CUDA 引擎)" << std::endl;
            } catch (...) {
                cuda_initialized = false;
                session_.reset();
            }
        }

        // 2. 若未检测到 NVIDIA 环境、驱动/CUDA 版本不满足或缺少组件，自动平滑推导为 CPU 模式
        if (!cuda_initialized) {
            try {
                Ort::SessionOptions cpu_options;
                unsigned int num_threads = std::max(2u, std::min(4u, std::thread::hardware_concurrency()));
                cpu_options.SetIntraOpNumThreads(num_threads);
                cpu_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

                session_ = std::make_unique<Ort::Session>(env_, wide_path.c_str(), cpu_options);
                backend_name_ = "CPU (多线程高性能优化)";
                std::cout << "[OnnxDetector] 未检测到匹配的 NVIDIA CUDA 运行库或显卡驱动版本不满足，系统已自动推导并平滑降级为 CPU 多线程模式 (线程数: "
                          << num_threads << ")" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[OnnxDetector] Failed to load model file on CPU: " << e.what() << std::endl;
                return false;
            }
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

        // 1. 等比 Letterbox 预处理至 640x640 (复用 canvas_mat_ 与 resized_mat_)
        float r = std::min((float)input_w_ / bgr_frame.cols, (float)input_h_ / bgr_frame.rows);
        int unpad_w = std::round(bgr_frame.cols * r);
        int unpad_h = std::round(bgr_frame.rows * r);
        int pad_w = (input_w_ - unpad_w) / 2;
        int pad_h = (input_h_ - unpad_h) / 2;

        canvas_mat_.setTo(cv::Scalar(114, 114, 114));
        cv::resize(bgr_frame, resized_mat_, cv::Size(unpad_w, unpad_h));
        resized_mat_.copyTo(canvas_mat_(cv::Rect(pad_w, pad_h, unpad_w, unpad_h)));

        // 2. 利用 OpenCV 原生 SIMD 加速的 blobFromImage 完成 BGR->RGB 转换、归一化(1/255)与通道重排(NCHW)
        // blob_mat_ 内部内存复用，彻底杜绝每帧 15MB 堆内存反复申请与释放
        cv::dnn::blobFromImage(canvas_mat_, blob_mat_, 1.0 / 255.0, cv::Size(input_w_, input_h_), cv::Scalar(), true, false, CV_32F);

        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info_, reinterpret_cast<float*>(blob_mat_.data), 3 * input_h_ * input_w_,
            input_shape_.data(), input_shape_.size()
        );

        // 3. 执行模型推理
        auto output_tensors = session_->Run(
            Ort::RunOptions{nullptr},
            input_names_ptrs_.data(), &input_tensor, 1,
            output_names_ptrs_.data(), output_names_ptrs_.size()
        );

        if (output_tensors.empty()) return false;

        // 4. 解析输出张量 (YOLOv8 形状: [1, 84, 8400])
        float* raw_output = output_tensors[0].GetTensorMutableData<float>();
        auto output_shape = output_tensors[0].GetTensorTypeAndShapeInfo().GetShape();
        if (output_shape.size() < 3) return false;

        int dimensions = static_cast<int>(output_shape[1]);  // 84
        int num_proposals = static_cast<int>(output_shape[2]); // 8400

        std::vector<cv::Rect> boxes;
        std::vector<float> confidences;
        boxes.reserve(256);
        confidences.reserve(256);

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

        // 5. OpenCV NMS 消除冗余重叠框
        std::vector<int> indices;
        cv::dnn::NMSBoxes(boxes, confidences, conf_thresh_, nms_thresh_, indices);

        detections.reserve(indices.size());
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
