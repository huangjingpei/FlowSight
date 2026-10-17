#pragma once
#include <opencv2/opencv.hpp>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <QDir>

class ClipRecorder {
public:
    explicit ClipRecorder(size_t max_buffer_frames = 90) : max_buffer_size_(max_buffer_frames) {}

    // 每帧送入画面构建环形缓冲（约 3 秒）
    void push_frame(const cv::Mat& frame) {
        if (frame.empty()) return;
        std::lock_guard<std::mutex> lock(mtx_);
        if (frame_buffer_.size() >= max_buffer_size_) {
            frame_buffer_.pop_front();
        }
        frame_buffer_.push_back(frame.clone());
    }

    // 触发进出事件时调用：异步保存切片
    std::string save_clip_async(const std::string& output_dir, int track_id, int64_t timestamp_ms, int fps = 25) {
        std::vector<cv::Mat> snapshot;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            snapshot.assign(frame_buffer_.begin(), frame_buffer_.end());
        }

        if (snapshot.empty()) return "";

        QDir dir(QString::fromStdString(output_dir));
        if (!dir.exists()) {
            dir.mkpath(".");
        }

        std::string filename = output_dir + "/clip_" + std::to_string(timestamp_ms) + 
                               "_t" + std::to_string(track_id) + ".mp4";

        std::thread([snapshot = std::move(snapshot), filename, fps]() {
            if (snapshot.empty()) return;
            int w = snapshot[0].cols;
            int h = snapshot[0].rows;
            // 尝试以 mp4v / avc1 编码保存
            cv::VideoWriter writer(filename, cv::VideoWriter::fourcc('m', 'p', '4', 'v'), fps, cv::Size(w, h));
            if (!writer.isOpened()) {
                writer.open(filename, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), fps, cv::Size(w, h));
            }
            for (const auto& f : snapshot) {
                writer.write(f);
            }
            writer.release();
        }).detach();

        return filename;
    }

private:
    size_t max_buffer_size_;
    std::deque<cv::Mat> frame_buffer_;
    std::mutex mtx_;
};
