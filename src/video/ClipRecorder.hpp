#pragma once
#include <opencv2/opencv.hpp>
#include <deque>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <memory>
#ifdef _WIN32
#include <objbase.h>
#endif
#include <QDir>
#include <QFileInfo>
#include <QDebug>

class ClipRecorder {
public:
    explicit ClipRecorder(size_t max_buffer_frames = 90) : max_buffer_size_(max_buffer_frames) {
        state_ = std::make_shared<SharedState>();
        state_->running.store(true);
        // 将 state_ 按值捕获传入后台工作线程，确保即使 ClipRecorder 对象本身被外部提前析构，
        // 后台线程所引用的锁、条件变量与任务队列依然合法存活，彻底根绝 Use-After-Free 崩溃
        worker_thread_ = std::thread([state = state_]() {
            worker_loop(state);
        });
    }

    ~ClipRecorder() {
        stop();
    }

    void stop() {
        if (!state_) return;

        bool expected = true;
        if (!state_->running.compare_exchange_strong(expected, false)) {
            // 已在停止中或已停止，若当前不在 worker_thread_ 内部，等待其安全退出
            if (worker_thread_.joinable() && std::this_thread::get_id() != worker_thread_.get_id()) {
                try {
                    worker_thread_.join();
                } catch (...) {}
            }
            return;
        }

        // 立即排空积压队列，杜绝阻塞主退出流程
        {
            std::lock_guard<std::mutex> lock(state_->task_mtx);
            while (!state_->task_queue.empty()) {
                state_->task_queue.pop();
            }
        }
        state_->cv.notify_all();

        if (worker_thread_.joinable() && std::this_thread::get_id() != worker_thread_.get_id()) {
            try {
                worker_thread_.join();
            } catch (...) {}
        }
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mtx_);
        frame_buffer_.clear();
    }

    // 每帧送入画面构建带时间戳的环形缓冲（约 3 秒）
    void push_frame(const cv::Mat& frame) {
        if (!state_ || !state_->running.load() || frame.empty() || frame.cols <= 0 || frame.rows <= 0) return;
        int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();

        std::lock_guard<std::mutex> lock(mtx_);
        if (frame_buffer_.size() >= max_buffer_size_) {
            frame_buffer_.pop_front();
        }
        // 关键内存保护：若画面分辨率超过 1280 宽度，下采样至 720p 存储切片池
        // 彻底根除 1080p 原画连续 clone 产生 500MB+ 内存暴涨崩溃
        if (frame.cols > 1280) {
            cv::Mat downscaled;
            float scale = 1280.0f / static_cast<float>(frame.cols);
            int target_w = 1280;
            int target_h = (static_cast<int>(frame.rows * scale) / 2) * 2; // 强制偶数尺寸，杜绝编码越界
            cv::resize(frame, downscaled, cv::Size(target_w, target_h));
            frame_buffer_.push_back(BufferedFrame{downscaled, now_ms});
        } else {
            frame_buffer_.push_back(BufferedFrame{frame.clone(), now_ms});
        }
    }

    // 触发进出事件时调用：将切片投递到任务队列，由后台独立线程平滑排队编码保存
    // 关键物理校准: 视频切片编码封装严格与摄像头原生采集帧率 (stream_fps, 默认25) 1:1 等速对齐，彻底根除回放“快进感”
    std::string save_clip_async(const std::string& output_dir, int track_id, int64_t timestamp_ms, int stream_fps = 25) {
        if (!state_ || !state_->running.load()) return "";
        std::vector<cv::Mat> snapshot;
        int actual_fps = (stream_fps >= 10 && stream_fps <= 60) ? stream_fps : 25;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (frame_buffer_.empty()) return "";

            snapshot.reserve(frame_buffer_.size());
            for (const auto& bf : frame_buffer_) {
                if (!bf.frame.empty() && bf.frame.cols > 0 && bf.frame.rows > 0) {
                    // cv::Mat 浅拷贝增加引用计数，零内存二次重分配
                    snapshot.push_back(bf.frame);
                }
            }
        }
        if (snapshot.empty()) return "";

        QDir dir(QString::fromStdString(output_dir));
        if (!dir.exists()) {
            dir.mkpath(".");
        }

        std::string filename = output_dir + "/clip_" + std::to_string(timestamp_ms) + 
                               "_t" + std::to_string(track_id) + ".mp4";

        {
            std::lock_guard<std::mutex> lock(state_->task_mtx);
            if (!state_->running.load()) return "";
            // 队列积压保护：限制最多 3 个切片排队，淘汰旧任务防范堆积
            while (state_->task_queue.size() >= 3) {
                state_->task_queue.pop();
            }
            state_->task_queue.push(ClipTask{std::move(snapshot), filename, actual_fps});
        }
        state_->cv.notify_one();

        return filename;
    }

private:
    struct BufferedFrame {
        cv::Mat frame;
        int64_t timestamp_ms = 0;
    };

    struct ClipTask {
        std::vector<cv::Mat> snapshot;
        std::string filename;
        int fps = 25;
    };

    // 独立生命周期共享状态结构体：供后台 Worker 线程持有，解耦父对象生命周期
    struct SharedState {
        std::atomic<bool> running{false};
        std::mutex task_mtx;
        std::condition_variable cv;
        std::queue<ClipTask> task_queue;
    };

    static void worker_loop(std::shared_ptr<SharedState> state) {
        if (!state) return;

#ifdef _WIN32
        struct ComGuard {
            HRESULT hr;
            ComGuard() { hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED); }
            ~ComGuard() { if (SUCCEEDED(hr)) CoUninitialize(); }
        } com_guard;
#endif

        while (state->running.load()) {
            ClipTask task;
            {
                std::unique_lock<std::mutex> lock(state->task_mtx);
                // 关键升级：使用 wait_for 增加 100ms 守护超时，配合 state 智能指针生命周期保活，
                // 彻底杜绝 lambda 捕获 dangling this 产生的 0xC0000005 崩溃与通知丢失死锁
                state->cv.wait_for(lock, std::chrono::milliseconds(100), [&state]() {
                    return !state->running.load() || !state->task_queue.empty();
                });

                if (!state->running.load()) {
                    break;
                }

                if (state->task_queue.empty()) {
                    continue;
                }

                task = std::move(state->task_queue.front());
                state->task_queue.pop();
            }

            if (task.snapshot.empty()) continue;

            try {
                // 1. 寻找首个非空有效帧确定切片基准分辨率
                int raw_w = 0, raw_h = 0;
                for (const auto& f : task.snapshot) {
                    if (!f.empty() && f.cols > 0 && f.rows > 0) {
                        raw_w = f.cols;
                        raw_h = f.rows;
                        break;
                    }
                }

                if (raw_w <= 0 || raw_h <= 0) {
                    qWarning() << "[ClipRecorder] 切片任务中无可用的有效画面尺寸，跳过:" << QString::fromStdString(task.filename);
                    continue;
                }

                // 强制对齐为偶数尺寸：视频编码（YUV420）必须为偶数分辨率，奇数尺寸会导致底层 memcpy 越界奔溃 (0xC0000005)
                int w = (raw_w / 2) * 2;
                int h = (raw_h / 2) * 2;
                if (w < 64 || h < 64) continue;

                // 2. 保证目标保存路径上层目录真实存在
                QFileInfo fi(QString::fromStdString(task.filename));
                QDir().mkpath(fi.dir().absolutePath());
                std::string native_path = QDir::toNativeSeparators(QString::fromStdString(task.filename)).toStdString();

                // 3. 稳健初始化 VideoWriter：优先使用 OpenCV 自带 FFmpeg 原生内置的 mp4v 编码器
                // 注意：绝不可在同一个 cv::VideoWriter 实例上连续循环 open/release，若前一次抛出异常，
                // 底层 COM/FFMPEG 句柄残存会导致下一次 release 触发 0xC0000005 访问冲突
                std::unique_ptr<cv::VideoWriter> writer;

                struct CodecAttempt {
                    int apiPreference;
                    int fourcc;
                };

                const std::vector<CodecAttempt> attempts = {
                    { cv::CAP_FFMPEG, cv::VideoWriter::fourcc('m', 'p', '4', 'v') }, // 黄金标准：FFmpeg 内置零依赖 MP4
                    { cv::CAP_ANY,    cv::VideoWriter::fourcc('m', 'p', '4', 'v') },
                    { cv::CAP_FFMPEG, cv::VideoWriter::fourcc('a', 'v', 'c', '1') },
                    { cv::CAP_ANY,    cv::VideoWriter::fourcc('M', 'J', 'P', 'G') }  // 终极保底：MJPG
                };

                for (const auto& att : attempts) {
                    try {
                        auto attempt_writer = std::make_unique<cv::VideoWriter>();
                        if (attempt_writer->open(native_path, att.apiPreference, att.fourcc, task.fps, cv::Size(w, h), true)) {
                            if (attempt_writer->isOpened()) {
                                writer = std::move(attempt_writer);
                                break;
                            }
                        }
                    } catch (...) {
                        // 局部 attempt_writer 自动作用域析构，绝不污染全局状态
                    }
                }

                // 关键核心防崩校验：若未能成功打开 VideoWriter，绝不可执行 writer->write()！
                if (!writer || !writer->isOpened()) {
                    qWarning() << "[ClipRecorder] 警告: 无法初始化 VideoWriter 编码器，跳过保存切片:"
                               << QString::fromStdString(native_path) << "分辨率:" << w << "x" << h << "FPS:" << task.fps;
                    continue;
                }

                // 4. 逐帧严格校准尺寸、通道与连续内存后安全写入，杜绝底层 ffmpeg sws_scale / memcpy 越界奔溃
                for (const auto& f : task.snapshot) {
                    if (!state->running.load()) break; // 退出检查
                    if (f.empty() || f.cols <= 0 || f.rows <= 0) continue;

                    cv::Mat ready_frame;
                    if (f.cols != w || f.rows != h) {
                        cv::resize(f, ready_frame, cv::Size(w, h));
                    } else {
                        ready_frame = f;
                    }

                    if (ready_frame.channels() == 1) {
                        cv::cvtColor(ready_frame, ready_frame, cv::COLOR_GRAY2BGR);
                    } else if (ready_frame.channels() == 4) {
                        cv::cvtColor(ready_frame, ready_frame, cv::COLOR_BGRA2BGR);
                    }

                    // 关键保护：强制保证数据内存连续性
                    if (!ready_frame.isContinuous()) {
                        ready_frame = ready_frame.clone();
                    }

                    try {
                        writer->write(ready_frame);
                    } catch (const std::exception& e) {
                        qWarning() << "[ClipRecorder] 写入视频帧异常:" << e.what();
                        break;
                    } catch (...) {
                        qWarning() << "[ClipRecorder] 写入视频帧发生未知底层异常，安全跳出写入循环";
                        break;
                    }
                }
                writer->release();
            } catch (const std::exception& e) {
                qWarning() << "[ClipRecorder] 处理切片任务异常:" << e.what();
            } catch (...) {
                qWarning() << "[ClipRecorder] 处理切片任务未知异常，继续维持工作线程运行";
            }
        }
    }

    size_t max_buffer_size_;
    std::deque<BufferedFrame> frame_buffer_;
    std::mutex mtx_;

    // 线程安全与生命周期解耦模型：使用 SharedState 确保即使 ClipRecorder 对象析构，
    // 后台工作线程依然安全持有合法资源，绝不访问野指针
    std::shared_ptr<SharedState> state_;
    std::thread worker_thread_;
};
