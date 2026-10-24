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
        if (frame.empty()) return;
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
                snapshot.push_back(bf.frame);
            }
        }

        QDir dir(QString::fromStdString(output_dir));
        if (!dir.exists()) {
            dir.mkpath(".");
        }

        std::string filename = output_dir + "/clip_" + std::to_string(timestamp_ms) + 
                               "_t" + std::to_string(track_id) + ".mp4";

        {
            std::lock_guard<std::mutex> lock(task_mtx_);
            // 队列积压保护：若超过 15 个编码任务在排队，直接饱和降级返回空，杜绝已入库任务被弹出导致证据回溯 404 死链
            if (task_queue_.size() >= 15) {
                return "";
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

            int w = task.snapshot[0].cols;
            int h = task.snapshot[0].rows;
            // 移动端 HTML5 <video> 强依赖 H.264 编码 (avc1 / H264)，优先使用 H.264，缺省时降级兼容
            cv::VideoWriter writer(task.filename, cv::VideoWriter::fourcc('a', 'v', 'c', '1'), task.fps, cv::Size(w, h));
            if (!writer.isOpened()) {
                writer.open(task.filename, cv::VideoWriter::fourcc('H', '2', '6', '4'), task.fps, cv::Size(w, h));
            }
            if (!writer.isOpened()) {
                writer.open(task.filename, cv::VideoWriter::fourcc('X', '2', '6', '4'), task.fps, cv::Size(w, h));
            }
            if (!writer.isOpened()) {
                writer.open(task.filename, cv::VideoWriter::fourcc('m', 'p', '4', 'v'), task.fps, cv::Size(w, h));
            }
            if (!writer.isOpened()) {
                writer.open(task.filename, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), task.fps, cv::Size(w, h));
            }
            for (const auto& f : task.snapshot) {
                writer.write(f);
            }
            writer.release();
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
