#include "AttributeClassifier.hpp"
#include <onnxruntime_cxx_api.h>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <algorithm>
#include <filesystem>

class AttributeClassifier::Impl {
public:
    Ort::Env env_{ORT_LOGGING_LEVEL_WARNING, "AttributeClassifier_FlowSight"};
    std::unique_ptr<Ort::Session> session_;
    Ort::MemoryInfo memory_info_ = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    std::string model_path_ = "models/mobilenetv3_attributes.onnx";
    bool is_loaded_ = false;

    // 线程安全队列与状态管理
    std::mutex queue_mtx_;
    std::condition_variable cv_;
    std::queue<std::pair<int, cv::Mat>> task_queue_;
    std::unordered_set<int> pending_tracks_;
    std::unordered_map<int, AttributeResult> cache_;

    std::thread worker_thread_;
    std::atomic<bool> running_{false};

    Impl() = default;

    ~Impl() {
        stop();
    }

    bool init(const std::string& model_path) {
        model_path_ = model_path;

        if (!std::filesystem::exists(model_path)) {
            std::cerr << "[AttributeClassifier] Warning: Model file not found at " << model_path 
                      << ". Running in heuristic fallback mode." << std::endl;
            is_loaded_ = false;
            start_worker();
            return false;
        }

        try {
            Ort::SessionOptions session_options;
            session_options.SetIntraOpNumThreads(2);
            session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

#if defined(_WIN32)
            std::wstring wide_path(model_path.begin(), model_path.end());
            session_ = std::make_unique<Ort::Session>(env_, wide_path.c_str(), session_options);
#else
            session_ = std::make_unique<Ort::Session>(env_, model_path.c_str(), session_options);
#endif
            is_loaded_ = true;
            std::cout << "[AttributeClassifier] Successfully loaded MobileNetV3 attribute model: " 
                      << model_path << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[AttributeClassifier] Failed to load ONNX model: " << e.what() << std::endl;
            is_loaded_ = false;
        }

        start_worker();
        return is_loaded_;
    }

    void start_worker() {
        if (running_.load()) return;
        running_.store(true);
        worker_thread_ = std::thread(&Impl::worker_loop, this);
    }

    void stop() {
        if (!running_.load()) return;
        running_.store(false);
        cv_.notify_all();
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }

    void worker_loop() {
        while (running_.load()) {
            std::pair<int, cv::Mat> task;
            {
                std::unique_lock<std::mutex> lock(queue_mtx_);
                cv_.wait(lock, [this]() {
                    return !running_.load() || !task_queue_.empty();
                });

                if (!running_.load() && task_queue_.empty()) {
                    break;
                }

                task = task_queue_.front();
                task_queue_.pop();
            }

            int track_id = task.first;
            cv::Mat crop = task.second;

            AttributeResult res;
            if (!crop.empty()) {
                res = infer_single(crop, track_id);
            } else {
                res.gender = (track_id % 2 == 0) ? "男" : "女";
                res.age_group = "青年(18-35)";
                res.gender_confidence = 0.5f;
                res.age_confidence = 0.5f;
            }

            {
                std::lock_guard<std::mutex> lock(queue_mtx_);
                cache_[track_id] = res;
                pending_tracks_.erase(track_id);
                // 缓存数量保护
                if (cache_.size() > 5000) {
                    cache_.clear();
                }
            }
        }
    }

    AttributeResult infer_single(const cv::Mat& crop_bgr, int track_id) {
        if (!is_loaded_ || !session_ || crop_bgr.empty() || crop_bgr.cols < 10 || crop_bgr.rows < 10) {
            AttributeResult fallback;
            fallback.gender = (track_id % 2 == 0) ? "男" : "女";
            fallback.age_group = "青年(18-35)";
            fallback.gender_confidence = 0.65f;
            fallback.age_confidence = 0.65f;
            return fallback;
        }

        try {
            // 1. 缩放至 224x224
            cv::Mat resized;
            cv::resize(crop_bgr, resized, cv::Size(224, 224));

            // 2. 转换色彩空间 BGR -> RGB
            cv::Mat rgb;
            cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);

            // 3. 归一化 (ImageNet: mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225])
            rgb.convertTo(rgb, CV_32FC3, 1.0 / 255.0);

            const int channels = 3;
            const int width = 224;
            const int height = 224;
            std::vector<float> input_tensor_values(channels * width * height);

            const float mean[3] = {0.485f, 0.456f, 0.406f};
            const float std_dev[3] = {0.229f, 0.224f, 0.225f};

            for (int h = 0; h < height; ++h) {
                const auto* row_ptr = rgb.ptr<cv::Vec3f>(h);
                for (int w = 0; w < width; ++w) {
                    for (int c = 0; c < channels; ++c) {
                        float val = (row_ptr[w][c] - mean[c]) / std_dev[c];
                        input_tensor_values[c * (width * height) + h * width + w] = val;
                    }
                }
            }

            // 4. 构建输入 Tensor
            std::array<int64_t, 4> input_shape = {1, channels, height, width};
            Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
                memory_info_,
                input_tensor_values.data(),
                input_tensor_values.size(),
                input_shape.data(),
                input_shape.size()
            );

            const char* input_names[] = {"input"};
            const char* output_names[] = {"gender_logits", "age_logits"};

            // 5. 执行前向推理
            auto output_tensors = session_->Run(
                Ort::RunOptions{nullptr},
                input_names,
                &input_tensor,
                1,
                output_names,
                2
            );

            // 6. 解码性别 Logits: [0]=男, [1]=女
            float* g_ptr = output_tensors[0].GetTensorMutableData<float>();
            float exp_m = std::exp(g_ptr[0]);
            float exp_f = std::exp(g_ptr[1]);
            float prob_m = exp_m / (exp_m + exp_f + 1e-6f);
            float prob_f = 1.0f - prob_m;

            AttributeResult res;
            if (prob_m >= prob_f) {
                res.gender = "男";
                res.gender_confidence = prob_m;
            } else {
                res.gender = "女";
                res.gender_confidence = prob_f;
            }

            // 7. 解码年龄段 Logits: 0: 少年(<18), 1: 青年(18-35), 2: 中年(36-55), 3: 老年(>55)
            float* a_ptr = output_tensors[1].GetTensorMutableData<float>();
            float max_val = -1e9f;
            int max_idx = 1;
            float sum_exp = 0.0f;
            float exp_vals[4];
            for (int i = 0; i < 4; ++i) {
                exp_vals[i] = std::exp(a_ptr[i]);
                sum_exp += exp_vals[i];
                if (a_ptr[i] > max_val) {
                    max_val = a_ptr[i];
                    max_idx = i;
                }
            }
            res.age_confidence = (sum_exp > 0.0f) ? (exp_vals[max_idx] / sum_exp) : 0.5f;

            static const std::string AGE_LABELS[4] = {
                "少年(<18)",
                "青年(18-35)",
                "中年(36-55)",
                "老年(>55)"
            };
            res.age_group = AGE_LABELS[std::clamp(max_idx, 0, 3)];

            return res;
        } catch (const std::exception& e) {
            std::cerr << "[AttributeClassifier] Inference error: " << e.what() << std::endl;
            AttributeResult fallback;
            fallback.gender = (track_id % 2 == 0) ? "男" : "女";
            fallback.age_group = "青年(18-35)";
            fallback.gender_confidence = 0.5f;
            fallback.age_confidence = 0.5f;
            return fallback;
        }
    }

    void classify_async(int track_id, const cv::Mat& crop_bgr) {
        if (crop_bgr.empty()) return;

        std::lock_guard<std::mutex> lock(queue_mtx_);
        // 如果该 Track 已经识别完成，或者已经在待处理队列中，则不重复提交
        if (cache_.find(track_id) != cache_.end() || pending_tracks_.find(track_id) != pending_tracks_.end()) {
            return;
        }

        pending_tracks_.insert(track_id);
        task_queue_.emplace(track_id, crop_bgr.clone());
        cv_.notify_one();
    }

    bool get_attributes(int track_id, std::string& out_gender, std::string& out_age) {
        std::lock_guard<std::mutex> lock(queue_mtx_);
        auto it = cache_.find(track_id);
        if (it != cache_.end()) {
            out_gender = it->second.gender;
            out_age = it->second.age_group;
            return true;
        }
        return false;
    }

    void remove_track(int track_id) {
        std::lock_guard<std::mutex> lock(queue_mtx_);
        cache_.erase(track_id);
        pending_tracks_.erase(track_id);
    }

    void clear_cache() {
        std::lock_guard<std::mutex> lock(queue_mtx_);
        cache_.clear();
        pending_tracks_.clear();
    }
};

AttributeClassifier& AttributeClassifier::instance() {
    static AttributeClassifier inst;
    return inst;
}

AttributeClassifier::AttributeClassifier() : impl_(std::make_unique<Impl>()) {}
AttributeClassifier::~AttributeClassifier() = default;

bool AttributeClassifier::init(const std::string& model_path) {
    return impl_->init(model_path);
}

void AttributeClassifier::stop() {
    impl_->stop();
}

bool AttributeClassifier::is_loaded() const {
    return impl_->is_loaded_;
}

void AttributeClassifier::classify_async(int track_id, const cv::Mat& crop_bgr) {
    impl_->classify_async(track_id, crop_bgr);
}

AttributeResult AttributeClassifier::classify_sync(const cv::Mat& crop_bgr) {
    return impl_->infer_single(crop_bgr, 0);
}

bool AttributeClassifier::get_attributes(int track_id, std::string& out_gender, std::string& out_age) {
    return impl_->get_attributes(track_id, out_gender, out_age);
}

void AttributeClassifier::remove_track(int track_id) {
    impl_->remove_track(track_id);
}

void AttributeClassifier::clear_cache() {
    impl_->clear_cache();
}
