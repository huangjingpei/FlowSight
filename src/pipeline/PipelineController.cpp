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
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <atomic>
#include <mutex>

static std::string resolve_resource_path(const std::string& path_str) {
    if (path_str.empty()) return path_str;
    if (path_str.find("://") != std::string::npos) return path_str; // RTSP/HTTP URL

    QFileInfo fi(QString::fromStdString(path_str));
    if (fi.isAbsolute() && fi.exists()) return path_str;
    if (fi.exists()) return QDir::cleanPath(fi.absoluteFilePath()).toStdString();

    QString app_dir = QCoreApplication::applicationDirPath();
    QString candidate1 = app_dir + "/" + QString::fromStdString(path_str);
    if (QFileInfo::exists(candidate1)) return QDir::cleanPath(candidate1).toStdString();

    QString candidate2 = app_dir + "/../../" + QString::fromStdString(path_str);
    if (QFileInfo::exists(candidate2)) return QDir::cleanPath(candidate2).toStdString();

    return path_str;
}

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

        std::string resolved_model = resolve_resource_path(model_path_);
        bool model_loaded = detector_->init(resolved_model, 0.45f, 0.50f);
        if (!model_loaded) {
            qWarning() << "[PipelineController] Model file not loaded from:" << QString::fromStdString(resolved_model);
        }

        // 打开视频源
        std::string resolved_video = resolve_resource_path(video_source_);
        cv::VideoCapture cap;
        bool has_real_video = false;
        if (!resolved_video.empty()) {
            has_real_video = cap.open(resolved_video);
            if (!has_real_video) {
                qWarning() << "[PipelineController] Could not open video source:" << QString::fromStdString(resolved_video);
            }
        }

        cv::Mat frame;
        QElapsedTimer fps_timer;
        fps_timer.start();
        int frame_count = 0;
        float current_fps = 30.0f;

        while (is_running_) {
            int64_t now_ms = QDateTime::currentMSecsSinceEpoch();

            if (!has_real_video) {
                // 无视频输入源时，向 UI 投递空帧，触发 VideoCanvas 高保真矢量待机界面 (零像素撕裂、零 Hershey 点阵字体)
                emit frame_ready(QImage(), {});
                StatisticsSnapshot snap;
                snap.fps = 0.0f;
                snap.infer_ms = 0.0f;
                snap.current_occupancy = 0;
                snap.today_entry = today_entry_;
                snap.today_exit = today_exit_;
                snap.today_total = today_entry_ + today_exit_;
                emit stats_updated(snap);
                QThread::msleep(80);
                continue;
            }

            if (!cap.read(frame) || frame.empty()) {
                // 循环读取本地视频文件
                cap.set(cv::CAP_PROP_POS_FRAMES, 0);
                if (!cap.read(frame) || frame.empty()) {
                    qWarning() << "[PipelineController] Stream disconnected or EOF, retrying...";
                    QThread::msleep(200);
                    cap.open(resolved_video);
                    continue;
                }
            }

            clip_recorder_->push_frame(frame);

            // 1. 目标检测 (真实模型推理)
            std::vector<Detection> detections;
            QElapsedTimer infer_timer;
            infer_timer.start();

            if (model_loaded) {
                detector_->detect(frame, detections);
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
