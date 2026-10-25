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
#include <QDir>
#include <QFileInfo>
#include <QDebug>

class ClipRecorder {
public:
    explicit ClipRecorder(size_t max_buffer_frames = 90) : max_buffer_size_(max_buffer_frames) {
        running_.store(true);
        worker_thread_ = std::thread(&ClipRecorder::worker_loop, this);
    }

    ~ClipRecorder() {
        stop();
    }

    void stop() {
        if (!running_.load()) return;
        running_.store(false);
        cv_.notify_all();
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }

    // 每帧送入画面构建带时间戳的环形缓冲（约 3 秒）
    void push_frame(const cv::Mat& frame) {
        if (frame.empty() || frame.cols <= 0 || frame.rows <= 0) return;
        int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();

        std::lock_guard<std::mutex> lock(mtx_);
        if (frame_buffer_.size() >= max_buffer_size_) {
            frame_buffer_.pop_front();
        }
        frame_buffer_.push_back(BufferedFrame{frame.clone(), now_ms});
    }

    // 触发进出事件时调用：将切片投递到任务队列，由后台独立线程平滑排队编码保存
    // 关键物理校准: 视频切片编码封装严格与摄像头原生采集帧率 (stream_fps, 默认25) 1:1 等速对齐，彻底根除回放“快进感”
    std::string save_clip_async(const std::string& output_dir, int track_id, int64_t timestamp_ms, int stream_fps = 25) {
        std::vector<cv::Mat> snapshot;
        int actual_fps = (stream_fps >= 10 && stream_fps <= 60) ? stream_fps : 25;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (frame_buffer_.empty()) return "";

            snapshot.reserve(frame_buffer_.size());
            for (const auto& bf : frame_buffer_) {
                if (!bf.frame.empty() && bf.frame.cols > 0 && bf.frame.rows > 0) {
                    // 严格深拷贝每一帧像素，杜绝与环形缓冲区并发弹出或复用产生内存引用冲突
                    snapshot.push_back(bf.frame.clone());
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
            std::lock_guard<std::mutex> lock(task_mtx_);
            // 队列积压保护：若排队任务过多，淘汰最旧任务，杜绝内存耗尽崩溃
            while (task_queue_.size() >= 10) {
                task_queue_.pop();
            }
            task_queue_.push(ClipTask{std::move(snapshot), filename, actual_fps});
        }
        cv_.notify_one();

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

    void worker_loop() {
        while (running_.load()) {
            ClipTask task;
            {
                std::unique_lock<std::mutex> lock(task_mtx_);
                cv_.wait(lock, [this]() {
                    return !running_.load() || !task_queue_.empty();
                });

                if (!running_.load() && task_queue_.empty()) {
                    break;
                }

                task = std::move(task_queue_.front());
                task_queue_.pop();
            }

            if (task.snapshot.empty()) continue;

            try {
                // 1. 寻找首个非空有效帧确定切片基准分辨率
                int w = 0, h = 0;
                for (const auto& f : task.snapshot) {
                    if (!f.empty() && f.cols > 0 && f.rows > 0) {
                        w = f.cols;
                        h = f.rows;
                        break;
                    }
                }
                if (w <= 0 || h <= 0) {
                    qWarning() << "[ClipRecorder] 切片任务中无可用的有效画面尺寸，跳过:" << QString::fromStdString(task.filename);
                    continue;
                }

                // 2. 保证目标保存路径上层目录真实存在
                QFileInfo fi(QString::fromStdString(task.filename));
                QDir().mkpath(fi.dir().absolutePath());
                std::string native_path = QDir::toNativeSeparators(QString::fromStdString(task.filename)).toStdString();

                // 3. 逐级尝试 H.264 / MP4 兼容编码器打开 VideoWriter
                cv::VideoWriter writer;
                const std::vector<int> codecs = {
                    cv::VideoWriter::fourcc('a', 'v', 'c', '1'),
                    cv::VideoWriter::fourcc('H', '2', '6', '4'),
                    cv::VideoWriter::fourcc('X', '2', '6', '4'),
                    cv::VideoWriter::fourcc('m', 'p', '4', 'v'),
                    cv::VideoWriter::fourcc('M', 'P', '4', 'V'),
                    cv::VideoWriter::fourcc('X', 'V', 'I', 'D')
                };

                for (int fourcc : codecs) {
                    try {
                        if (writer.open(native_path, cv::CAP_FFMPEG, fourcc, task.fps, cv::Size(w, h), true)) {
                            if (writer.isOpened()) break;
                        }
                    } catch (...) {}
                    try {
                        if (writer.open(native_path, fourcc, task.fps, cv::Size(w, h), true)) {
                            if (writer.isOpened()) break;
                        }
                    } catch (...) {}
                }

                // 兜底尝试 MJPG 编码
                if (!writer.isOpened()) {
                    try {
                        writer.open(native_path, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), task.fps, cv::Size(w, h), true);
                    } catch (...) {}
                }

                // 关键核心防崩校验：若未能成功打开 VideoWriter，绝不可执行 writer.write()！
                if (!writer.isOpened()) {
                    qWarning() << "[ClipRecorder] 警告: 无法初始化 VideoWriter 编码器，跳过保存切片:"
                               << QString::fromStdString(native_path) << "分辨率:" << w << "x" << h << "FPS:" << task.fps;
                    continue;
                }

                // 4. 逐帧严格校准尺寸与通道后安全写入，杜绝底层 ffmpeg sws_scale / memcpy 越界奔溃
                for (const auto& f : task.snapshot) {
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

                    try {
                        writer.write(ready_frame);
                    } catch (const std::exception& e) {
                        qWarning() << "[ClipRecorder] 写入视频帧异常:" << e.what();
                        break;
                    } catch (...) {
                        qWarning() << "[ClipRecorder] 写入视频帧发生未知底层异常，安全跳出写入循环";
                        break;
                    }
                }
                writer.release();
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

    // 线程池队列与后台编码 Worker
    std::queue<ClipTask> task_queue_;
    std::mutex task_mtx_;
    std::condition_variable cv_;
    std::thread worker_thread_;
    std::atomic<bool> running_{false};
};
