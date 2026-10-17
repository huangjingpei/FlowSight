#include "PipelineController.hpp"
#include "ai/OnnxDetector.hpp"
#include "tracking/ByteTrackWrapper.hpp"
#include "analytics/RoiCounter.hpp"
#include "storage/DatabaseManager.hpp"
#include "video/ClipRecorder.hpp"

#include <opencv2/opencv.hpp>
#include <QDateTime>
#include <QElapsedTimer>
#include <QDebug>
#include <atomic>
#include <mutex>

class PipelineController::Worker : public QObject {
    Q_OBJECT
public:
    std::atomic<bool> is_running_{false};
    std::string model_path_;
    std::string video_source_;

    std::unique_ptr<IDetector> detector_;
    std::unique_ptr<ITracker> tracker_;
    std::unique_ptr<RoiCounter> roi_counter_;
    std::unique_ptr<ClipRecorder> clip_recorder_;

    std::mutex roi_mtx_;
    RoiConfig active_roi_;

    int today_entry_ = 0;
    int today_exit_ = 0;

    void set_roi(const RoiConfig& cfg) {
        std::lock_guard<std::mutex> lock(roi_mtx_);
        active_roi_ = cfg;
        if (roi_counter_) {
            roi_counter_->set_roi_config(std::make_shared<const RoiConfig>(cfg));
        }
    }

    RoiConfig get_roi() {
        std::lock_guard<std::mutex> lock(roi_mtx_);
        return active_roi_;
    }

public slots:
    void process() {
        is_running_ = true;
        detector_ = std::make_unique<OnnxDetector>();
        tracker_ = std::make_unique<ByteTrackWrapper>(30, 30);
        roi_counter_ = std::make_unique<RoiCounter>();
        clip_recorder_ = std::make_unique<ClipRecorder>(90);

        bool model_loaded = detector_->init(model_path_, 0.45f, 0.50f);
        if (!model_loaded) {
            qWarning() << "[PipelineController] Model file not loaded from:" << QString::fromStdString(model_path_);
        }

        // 打开视频源
        cv::VideoCapture cap;
        bool has_real_video = false;
        if (!video_source_.empty()) {
            has_real_video = cap.open(video_source_);
            if (!has_real_video) {
                qWarning() << "[PipelineController] Could not open video source:" << QString::fromStdString(video_source_);
            }
        }

        cv::Mat frame;
        QElapsedTimer fps_timer;
        fps_timer.start();
        int frame_count = 0;
        float current_fps = 30.0f;

        while (is_running_) {
            int64_t now_ms = QDateTime::currentMSecsSinceEpoch();

            if (has_real_video) {
                if (!cap.read(frame) || frame.empty()) {
                    // 循环读取本地视频文件
                    cap.set(cv::CAP_PROP_POS_FRAMES, 0);
                    if (!cap.read(frame) || frame.empty()) {
                        qWarning() << "[PipelineController] Stream disconnected or EOF, retrying...";
                        QThread::msleep(200);
                        cap.open(video_source_);
                        continue;
                    }
                }
            } else {
                // 无视频输入源时呈现专业待机画幅 (零虚假行人、零伪造检测)
                frame = cv::Mat(720, 1280, CV_8UC3, cv::Scalar(18, 22, 28));
                cv::putText(frame, "FlowSight AI Vision - Standby", cv::Point(80, 260),
                            cv::FONT_HERSHEY_SIMPLEX, 1.2, cv::Scalar(88, 166, 255), 2);
                cv::putText(frame, "No Active Video Stream Connected", cv::Point(80, 320),
                            cv::FONT_HERSHEY_SIMPLEX, 0.9, cv::Scalar(200, 200, 200), 2);
                cv::putText(frame, "Click [ Load MP4 Video... ] above to analyze real MP4 file or RTSP stream.", cv::Point(80, 380),
                            cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(139, 148, 158), 1);
                if (model_loaded) {
                    cv::putText(frame, "[Status] YOLOv8n ONNX Model: Ready (models/yolov8n.onnx)", cv::Point(80, 440),
                                cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(63, 185, 80), 2);
                } else {
                    cv::putText(frame, "[Status] YOLOv8n ONNX Model: Missing in models/yolov8n.onnx", cv::Point(80, 440),
                                cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(248, 81, 73), 2);
                }
                QThread::msleep(100);
            }

            clip_recorder_->push_frame(frame);

            // 1. 目标检测 (真实模型推理)
            std::vector<Detection> detections;
            QElapsedTimer infer_timer;
            infer_timer.start();

            if (model_loaded && has_real_video) {
                detector_->detect(frame, detections);
            } else if (!model_loaded && has_real_video) {
                cv::putText(frame, "Model not loaded! Place yolov8n.onnx in models/ folder.", cv::Point(30, 40),
                            cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 255), 2);
            }
            float infer_ms = static_cast<float>(infer_timer.nsecsElapsed()) / 1e6f;

            // 2. 目标跟踪 (ByteTrack 跟踪真实检测框)
            auto tracks = tracker_->update(detections);

            // 3. 区域滞后防抖计数
            int current_occupancy = 0;
            std::vector<PassageEvent> events;
            roi_counter_->process_frame(tracks, now_ms, current_occupancy, events);

            // 4. 事件触发与短视频切片异步保存
            for (auto& ev : events) {
                if (ev.direction == PassageDirection::ENTRY) today_entry_++;
                else if (ev.direction == PassageDirection::EXIT) today_exit_++;

                // 异步截取保存前后 3 秒 MP4 切片
                std::string clip = clip_recorder_->save_clip_async("clips", ev.track_id, ev.timestamp_ms);
                ev.clip_path = clip;

                // 持久化到 SQLite
                DatabaseManager::instance().insert_events({ev});
                emit event_occurred(ev);
            }

            // 5. 帧率统计与信号回传
            frame_count++;
            if (fps_timer.elapsed() >= 1000) {
                current_fps = frame_count * 1000.0f / fps_timer.elapsed();
                frame_count = 0;
                fps_timer.restart();

                StatisticsSnapshot snap;
                snap.current_occupancy = current_occupancy;
                snap.today_entry = today_entry_;
                snap.today_exit = today_exit_;
                snap.today_total = today_entry_ + today_exit_;
                snap.fps = current_fps;
                snap.infer_ms = infer_ms;
                emit stats_updated(snap);
            }

            // 将 Mat 转为 QImage
            QImage qimg(frame.data, frame.cols, frame.rows, static_cast<int>(frame.step), QImage::Format_BGR888);
            emit frame_ready(qimg.copy(), tracks);

            QThread::msleep(has_real_video ? 15 : 30);
        }

        if (has_real_video) cap.release();
    }

signals:
    void frame_ready(const QImage& frame, const std::vector<TrackObject>& tracks);
    void stats_updated(const StatisticsSnapshot& stats);
    void event_occurred(const PassageEvent& event);
};

PipelineController::PipelineController(QObject* parent) : QObject(parent) {
    qRegisterMetaType<std::vector<TrackObject>>("std::vector<TrackObject>");
    qRegisterMetaType<StatisticsSnapshot>("StatisticsSnapshot");
    qRegisterMetaType<PassageEvent>("PassageEvent");
}

PipelineController::~PipelineController() {
    stop_pipeline();
}

void PipelineController::start_pipeline(const std::string& model_path, const std::string& video_source) {
    if (worker_thread_.isRunning()) return;

    worker_ = std::make_unique<Worker>();
    worker_->model_path_ = model_path;
    worker_->video_source_ = video_source;
    worker_->moveToThread(&worker_thread_);

    connect(&worker_thread_, &QThread::started, worker_.get(), &Worker::process);
    connect(worker_.get(), &Worker::frame_ready, this, &PipelineController::frame_ready);
    connect(worker_.get(), &Worker::stats_updated, this, &PipelineController::stats_updated);
    connect(worker_.get(), &Worker::event_occurred, this, &PipelineController::event_occurred);

    // 默认恢复最新保存的 ROI
    auto saved_rois = DatabaseManager::instance().load_latest_roi_configs();
    if (!saved_rois.empty()) {
        worker_->set_roi(saved_rois.front());
    }

    worker_thread_.start();
}

void PipelineController::stop_pipeline() {
    if (worker_) {
        worker_->is_running_ = false;
    }
    if (worker_thread_.isRunning()) {
        worker_thread_.quit();
        worker_thread_.wait(2000);
    }
}

void PipelineController::set_video_source(const std::string& video_source) {
    std::string model = "models/yolov8n.onnx";
    if (worker_ && !worker_->model_path_.empty()) {
        model = worker_->model_path_;
    }
    stop_pipeline();
    start_pipeline(model, video_source);
}

void PipelineController::update_roi_config(const RoiConfig& config) {
    if (worker_) {
        worker_->set_roi(config);
    }
    DatabaseManager::instance().save_roi_config(config);
}

RoiConfig PipelineController::get_current_roi() const {
    if (worker_) {
        return worker_->get_roi();
    }
    return RoiConfig();
}

#include "PipelineController.moc"
