#include "PipelineController.hpp"
#include "ai/OnnxDetector.hpp"
#include "tracking/ByteTrackWrapper.hpp"
#include "analytics/RoiCounter.hpp"
#include "storage/DatabaseManager.hpp"
#include "storage/ConfigManager.hpp"
#include "video/ClipRecorder.hpp"

#include <opencv2/opencv.hpp>
#include <QDateTime>
#include <QElapsedTimer>
#include <QDebug>
#include <QCoreApplication>
#include <QApplication>
#include <QFileInfo>
#include <QDir>
#include <atomic>
#include <mutex>

static std::string resolve_resource_path(const std::string& path_str) {
    if (path_str.empty()) return path_str;
    if (path_str.find("://") != std::string::npos) return path_str;

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

static bool open_video_capture(cv::VideoCapture& cap, const std::string& path_str) {
    if (path_str.empty()) return false;

    // 1. 网络流 (RTSP / RTMP / HTTP)
    if (path_str.find("://") != std::string::npos) {
        if (cap.open(path_str, cv::CAP_FFMPEG)) return true;
        if (cap.open(path_str, cv::CAP_ANY)) return true;
        return false;
    }

    // 2. 本地文件 - 优先使用 FFMPEG 后端与 UTF-8
    if (cap.open(path_str, cv::CAP_FFMPEG)) return true;
    if (cap.open(path_str, cv::CAP_ANY)) return true;

#if defined(_WIN32)
    QString qpath = QString::fromStdString(path_str);

    // 尝试 Windows 原生反斜杠路径 (如 D:\音频+视频\MP4视频\001桃子.mp4)
    std::string native_utf8 = QDir::toNativeSeparators(qpath).toStdString();
    if (native_utf8 != path_str) {
        if (cap.open(native_utf8, cv::CAP_FFMPEG)) return true;
        if (cap.open(native_utf8, cv::CAP_ANY)) return true;
    }

    // 若 OpenCV 使用 MSMF 后端，需本地 ANSI 编码
    std::string ansi_path = qpath.toLocal8Bit().toStdString();
    if (ansi_path != path_str) {
        if (cap.open(ansi_path, cv::CAP_MSMF)) return true;
        if (cap.open(ansi_path, cv::CAP_ANY)) return true;
    }

    std::string native_ansi = QDir::toNativeSeparators(qpath).toLocal8Bit().toStdString();
    if (native_ansi != ansi_path) {
        if (cap.open(native_ansi, cv::CAP_MSMF)) return true;
        if (cap.open(native_ansi, cv::CAP_ANY)) return true;
    }
#endif

    return false;
}

class PipelineController::Worker : public QObject {
    Q_OBJECT
public:
    std::atomic<bool> is_running_{false};
    std::atomic<bool> is_paused_{false};
    std::atomic<bool> seek_requested_{false};
    std::atomic<int64_t> seek_target_ms_{0};
    std::atomic<int> step_requested_{0};
    std::atomic<double> playback_speed_{1.0};

    std::string model_path_;
    std::string video_source_;
    std::mutex source_mtx_;
    std::string pending_video_source_;
    std::atomic<bool> source_changed_{false};

    std::unique_ptr<IDetector> detector_;
    std::unique_ptr<ITracker> tracker_;
    std::unique_ptr<RoiCounter> roi_counter_;
    std::unique_ptr<ClipRecorder> clip_recorder_;

    std::mutex roi_mtx_;
    std::vector<RoiConfig> active_rois_;
    RoiConfig active_roi_;

    int today_entry_ = 0;
    int today_exit_ = 0;

    bool is_surged_ = false;
    int surge_confirm_count_ = 0;
    int last_surge_occupancy_ = 0;
    int last_w_ = 0;
    int last_h_ = 0;

    void set_rois(const std::vector<RoiConfig>& rois) {
        std::lock_guard<std::mutex> lock(roi_mtx_);
        active_rois_ = rois;
        if (!active_rois_.empty()) active_roi_ = active_rois_.front();
        if (roi_counter_) {
            if (last_w_ > 0 && last_h_ > 0) {
                std::vector<RoiConfig> adapted = adapt_rois_to_resolution(rois, last_w_, last_h_);
                roi_counter_->set_roi_configs(adapted);
            } else {
                roi_counter_->set_roi_configs(rois);
            }
        }
    }

    std::vector<RoiConfig> get_rois() {
        std::lock_guard<std::mutex> lock(roi_mtx_);
        return active_rois_;
    }

    void set_roi(const RoiConfig& cfg) {
        set_rois({cfg});
    }

    RoiConfig get_roi() {
        std::lock_guard<std::mutex> lock(roi_mtx_);
        return active_roi_;
    }

    void change_video_source(const std::string& new_source) {
        std::lock_guard<std::mutex> lock(source_mtx_);
        pending_video_source_ = new_source;
        source_changed_ = true;
    }

    void pause() { is_paused_ = true; }
    void resume() { is_paused_ = false; }
    void seek(qint64 pos_ms) {
        seek_target_ms_ = pos_ms;
        seek_requested_ = true;
    }
    void step(int delta_ms) {
        step_requested_ = delta_ms;
    }
    void set_speed(double speed) {
        if (speed > 0.05 && speed <= 5.0) {
            playback_speed_ = speed;
        }
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

        // 开机恢复今日已入库的进出累计客流数
        DatabaseManager::instance().get_today_counts(today_entry_, today_exit_);

        std::string cur_source = video_source_;
        std::string resolved_video = resolve_resource_path(cur_source);
        cv::VideoCapture cap;
        bool has_real_video = false;
        double video_fps = 30.0;
        double frame_count = 0.0;
        bool is_live = false;
        qint64 total_duration_ms = 0;

        auto init_capture = [&](const std::string& source_path) {
            if (cap.isOpened()) cap.release();
            resolved_video = resolve_resource_path(source_path);
            has_real_video = open_video_capture(cap, resolved_video);

            if (has_real_video) {
                video_fps = cap.get(cv::CAP_PROP_FPS);
                if (video_fps <= 0.0 || video_fps > 120.0) video_fps = 30.0;
                frame_count = cap.get(cv::CAP_PROP_FRAME_COUNT);

                is_live = (source_path.find("rtsp://") != std::string::npos ||
                           source_path.find("rtmp://") != std::string::npos ||
                           frame_count <= 0);

                total_duration_ms = is_live ? 0 : static_cast<qint64>((frame_count / video_fps) * 1000.0);
                emit media_info_ready(is_live, total_duration_ms, video_fps);
            } else {
                emit media_info_ready(false, 0, 30.0);
            }
        };

        if (!cur_source.empty()) {
            init_capture(cur_source);
        }

        cv::Mat frame;
        QElapsedTimer fps_timer;
        fps_timer.start();
        int frame_count_stat = 0;
        float current_fps = 30.0f;

        while (is_running_) {
            // 1. 检查是否切换视频源
            if (source_changed_) {
                source_changed_ = false;
                std::string next_source;
                {
                    std::lock_guard<std::mutex> lock(source_mtx_);
                    next_source = pending_video_source_;
                }
                cur_source = next_source;
                init_capture(cur_source);
            }

            // 2. 暂停与单帧步进处理
            if (is_paused_) {
                int step = step_requested_.exchange(0);
                if (step != 0 && has_real_video) {
                    double cur_frame = cap.get(cv::CAP_PROP_POS_FRAMES);
                    double target_frame = cur_frame + (step > 0 ? 1 : -2);
                    if (target_frame < 0) target_frame = 0;
                    cap.set(cv::CAP_PROP_POS_FRAMES, target_frame);
                    if (cap.read(frame) && !frame.empty()) {
                        qint64 pos_ms = static_cast<qint64>(cap.get(cv::CAP_PROP_POS_MSEC));
                        emit position_changed(pos_ms);

                        // 运行单帧分析并投递
                        std::vector<Detection> detections;
                        if (model_loaded) detector_->detect(frame, detections);
                        auto tracks = tracker_->update(detections);
                        QImage qimg(frame.data, frame.cols, frame.rows, static_cast<int>(frame.step), QImage::Format_BGR888);
                        emit frame_ready(qimg.copy(), tracks);
                    }
                }
                QThread::msleep(40);
                continue;
            }

            // 3. 拖拽快进 / 跳转 Seek
            if (seek_requested_) {
                seek_requested_ = false;
                int64_t target_ms = seek_target_ms_.load();
                if (has_real_video && !is_live) {
                    double target_frame = (target_ms * video_fps) / 1000.0;
                    if (target_frame < 0) target_frame = 0;
                    if (frame_count > 0 && target_frame >= frame_count) target_frame = frame_count - 1;
                    cap.set(cv::CAP_PROP_POS_FRAMES, target_frame);
                }
            }

            int64_t now_ms = QDateTime::currentMSecsSinceEpoch();

            if (!has_real_video) {
                emit frame_ready(QImage(), {});
                StatisticsSnapshot snap;
                snap.fps = 0.0f;
                snap.infer_ms = 0.0f;
                snap.current_occupancy = 0;
                snap.today_entry = today_entry_;
                snap.today_exit = today_exit_;
                snap.today_total = today_entry_ + today_exit_;
                if (detector_) snap.backend_name = detector_->get_backend_name();
                emit stats_updated(snap);
                QThread::msleep(80);
                continue;
            }

            // 4. 读取视频帧
            if (!cap.read(frame) || frame.empty()) {
                if (!is_live) {
                    // 本地视频循环播放
                    cap.set(cv::CAP_PROP_POS_FRAMES, 0);
                    emit loop_restarted();
                    if (!cap.read(frame) || frame.empty()) {
                        QThread::msleep(40);
                        continue;
                    }
                } else {
                    qWarning() << "[PipelineController] Stream disconnected, reconnecting...";
                    QThread::msleep(200);
                    init_capture(cur_source);
                    continue;
                }
            }

            // 报告当前播放进度 (毫秒)
            qint64 cur_pos_ms = static_cast<qint64>(cap.get(cv::CAP_PROP_POS_MSEC));
            emit position_changed(cur_pos_ms);

            clip_recorder_->push_frame(frame);

            // 检查视频分辨率并自适应调整 ROI 防区 (若尚未标定则自适应开箱默认防区，支持多防区自适应)
            if (frame.cols != last_w_ || frame.rows != last_h_) {
                last_w_ = frame.cols;
                last_h_ = frame.rows;
                std::vector<RoiConfig> adapted_list;
                {
                    std::lock_guard<std::mutex> lock(roi_mtx_);
                    if (active_rois_.empty()) {
                        active_rois_ = DatabaseManager::instance().load_latest_roi_configs();
                    }
                    adapted_list = adapt_rois_to_resolution(active_rois_, last_w_, last_h_);
                    active_rois_ = adapted_list;
                    if (!active_rois_.empty()) active_roi_ = active_rois_.front();
                    roi_counter_->set_roi_configs(adapted_list);
                }
                emit rois_auto_initialized(adapted_list);
                if (!adapted_list.empty()) {
                    emit roi_auto_initialized(adapted_list.front());
                }
            }

            // 5. 目标检测 (真实模型推理)
            std::vector<Detection> detections;
            QElapsedTimer infer_timer;
            infer_timer.start();

            if (model_loaded) {
                detector_->detect(frame, detections);
            }
            float infer_ms = static_cast<float>(infer_timer.nsecsElapsed()) / 1e6f;

            // 6. 目标跟踪 (ByteTrack)
            auto tracks = tracker_->update(detections);

            // 7. 区域滞后防抖计数
            int current_occupancy = 0;
            std::vector<PassageEvent> events;
            roi_counter_->process_frame(tracks, now_ms, current_occupancy, events);

            // 7.1 区域客流超载与拥挤度实时风控研判
            auto& cfg = ConfigManager::instance();
            bool surge_enabled = cfg.get_crowd_surge_enabled();
            int surge_limit = cfg.get_crowd_surge_threshold();

            if (surge_enabled && surge_limit > 0) {
                if (current_occupancy >= surge_limit) {
                    if (!is_surged_ || last_surge_occupancy_ != current_occupancy) {
                        is_surged_ = true;
                        last_surge_occupancy_ = current_occupancy;
                        if (cfg.get_crowd_surge_beep()) {
                            QApplication::beep();
                        }
                        emit crowd_surge_status_changed(true, current_occupancy, surge_limit);
                    }
                    surge_confirm_count_ = 0;
                } else {
                    if (is_surged_) {
                        // 防抖：连续 15 帧（约 0.5 秒）低于阈值才平滑解除
                        if (++surge_confirm_count_ >= 15) {
                            is_surged_ = false;
                            surge_confirm_count_ = 0;
                            last_surge_occupancy_ = current_occupancy;
                            emit crowd_surge_status_changed(false, current_occupancy, surge_limit);
                        }
                    }
                }
            } else if (is_surged_) {
                is_surged_ = false;
                surge_confirm_count_ = 0;
                last_surge_occupancy_ = 0;
                emit crowd_surge_status_changed(false, current_occupancy, surge_limit);
            }

            // 8. 通行事件与短切片异步保存
            bool has_new_events = !events.empty();
            for (auto& ev : events) {
                if (ev.direction == PassageDirection::ENTRY) today_entry_++;
                else if (ev.direction == PassageDirection::EXIT) today_exit_++;

                std::string clip = clip_recorder_->save_clip_async("clips", ev.track_id, ev.timestamp_ms);
                ev.clip_path = clip;
                DatabaseManager::instance().insert_events({ev});
                emit event_occurred(ev);
            }

            if (has_new_events) {
                StatisticsSnapshot snap;
                snap.current_occupancy = current_occupancy;
                snap.today_entry = today_entry_;
                snap.today_exit = today_exit_;
                snap.today_total = today_entry_ + today_exit_;
                snap.fps = current_fps;
                snap.infer_ms = infer_ms;
                if (detector_) snap.backend_name = detector_->get_backend_name();
                emit stats_updated(snap);
            }

            // 9. 帧率统计与状态信号
            frame_count_stat++;
            if (fps_timer.elapsed() >= 1000) {
                current_fps = frame_count_stat * 1000.0f / fps_timer.elapsed();
                frame_count_stat = 0;
                fps_timer.restart();

                StatisticsSnapshot snap;
                snap.current_occupancy = current_occupancy;
                snap.today_entry = today_entry_;
                snap.today_exit = today_exit_;
                snap.today_total = today_entry_ + today_exit_;
                snap.fps = current_fps;
                snap.infer_ms = infer_ms;
                if (detector_) snap.backend_name = detector_->get_backend_name();
                emit stats_updated(snap);
            }

            // 投递渲染帧到 VideoCanvas
            QImage qimg(frame.data, frame.cols, frame.rows, static_cast<int>(frame.step), QImage::Format_BGR888);
            emit frame_ready(qimg.copy(), tracks);

            // 10. 精准音画同步与帧率节奏控制
            double nominal_frame_ms = 1000.0 / video_fps;
            double speed = playback_speed_.load();
            int target_sleep_ms = static_cast<int>((nominal_frame_ms - infer_ms) / speed);
            if (target_sleep_ms > 0) {
                QThread::msleep(std::min(target_sleep_ms, 100));
            } else {
                QThread::msleep(1);
            }
        }

        if (has_real_video) cap.release();
    }

signals:
    void frame_ready(const QImage& frame, const std::vector<TrackObject>& tracks);
    void stats_updated(const StatisticsSnapshot& stats);
    void event_occurred(const PassageEvent& event);
    void media_info_ready(bool is_live, qint64 duration_ms, double fps);
    void position_changed(qint64 position_ms);
    void loop_restarted();
    void crowd_surge_status_changed(bool is_surged, int current_occupancy, int threshold);
    void roi_auto_initialized(const RoiConfig& config);
    void rois_auto_initialized(const std::vector<RoiConfig>& configs);
};

PipelineController::PipelineController(QObject* parent) : QObject(parent) {
    qRegisterMetaType<std::vector<TrackObject>>("std::vector<TrackObject>");
    qRegisterMetaType<StatisticsSnapshot>("StatisticsSnapshot");
    qRegisterMetaType<PassageEvent>("PassageEvent");
    qRegisterMetaType<RoiConfig>("RoiConfig");
    qRegisterMetaType<std::vector<RoiConfig>>("std::vector<RoiConfig>");

    // 初始化硬件音频声道
    audio_player_ = new QMediaPlayer(this);
    audio_output_ = new QAudioOutput(this);
    audio_output_->setVolume(0.8f);
    audio_player_->setAudioOutput(audio_output_);
}

PipelineController::~PipelineController() {
    stop_pipeline();
}

void PipelineController::start_pipeline(const std::string& model_path, const std::string& video_source) {
    worker_ = std::make_unique<Worker>();
    worker_->model_path_ = model_path;
    worker_->video_source_ = video_source;
    worker_->moveToThread(&worker_thread_);

    connect(&worker_thread_, &QThread::started, worker_.get(), &Worker::process);
    connect(worker_.get(), &Worker::frame_ready, this, &PipelineController::frame_ready);
    connect(worker_.get(), &Worker::stats_updated, this, &PipelineController::stats_updated);
    connect(worker_.get(), &Worker::event_occurred, this, &PipelineController::event_occurred);
    connect(worker_.get(), &Worker::media_info_ready, this, [this](bool is_live, qint64 d, double fps) {
        is_live_ = is_live;
        duration_ms_ = d;
        emit media_info_ready(is_live, d, fps);
    });
    connect(worker_.get(), &Worker::position_changed, this, &PipelineController::position_changed);
    connect(worker_.get(), &Worker::loop_restarted, this, &PipelineController::on_worker_loop_restarted);
    connect(worker_.get(), &Worker::crowd_surge_status_changed, this, &PipelineController::crowd_surge_status_changed);
    connect(worker_.get(), &Worker::roi_auto_initialized, this, &PipelineController::roi_auto_initialized);
    connect(worker_.get(), &Worker::rois_auto_initialized, this, &PipelineController::rois_auto_initialized);

    worker_thread_.start();

    // 启动初始媒体音频 (若为本地视频文件)
    set_video_source(video_source);
}

void PipelineController::stop_pipeline() {
    if (audio_player_) {
        audio_player_->stop();
    }
    if (worker_) {
        worker_->is_running_ = false;
        worker_thread_.quit();
        worker_thread_.wait(1000);
        worker_.reset();
    }
}

void PipelineController::set_video_source(const std::string& video_source) {
    if (worker_) {
        worker_->change_video_source(video_source);
    }

    // 检查是否为包含音频轨道的本地媒体文件
    std::string resolved = resolve_resource_path(video_source);
    is_live_ = (video_source.find("rtsp://") != std::string::npos ||
                video_source.find("rtmp://") != std::string::npos);

    if (audio_player_) {
        if (!is_live_ && !resolved.empty()) {
            QUrl media_url = QUrl::fromLocalFile(QString::fromStdString(resolved));
            audio_player_->setSource(media_url);
            audio_player_->play();
        } else {
            audio_player_->stop();
        }
    }
}

void PipelineController::update_roi_config(const RoiConfig& config) {
    update_roi_configs({config});
}

void PipelineController::update_roi_configs(const std::vector<RoiConfig>& configs) {
    if (worker_) {
        worker_->set_rois(configs);
    }
}

RoiConfig PipelineController::get_current_roi() const {
    auto list = get_current_rois();
    if (!list.empty()) return list.front();
    return {};
}

std::vector<RoiConfig> PipelineController::get_current_rois() const {
    if (worker_) {
        return worker_->get_rois();
    }
    return {};
}

void PipelineController::play() {
    if (worker_) worker_->resume();
    if (audio_player_ && !is_live_) audio_player_->play();
    emit playback_state_changed(true);
}

void PipelineController::pause() {
    if (worker_) worker_->pause();
    if (audio_player_) audio_player_->pause();
    emit playback_state_changed(false);
}

void PipelineController::stop() {
    if (worker_) {
        worker_->pause();
        worker_->seek(0);
    }
    if (audio_player_) {
        audio_player_->stop();
    }
    emit playback_state_changed(false);
    emit position_changed(0);
}

void PipelineController::seek(qint64 position_ms) {
    if (worker_) worker_->seek(position_ms);
    if (audio_player_ && !is_live_) audio_player_->setPosition(position_ms);
}

void PipelineController::step_forward(int ms) {
    if (worker_) worker_->step(ms);
}

void PipelineController::step_backward(int ms) {
    if (worker_) worker_->step(-ms);
}

void PipelineController::set_volume(int volume_0_100) {
    if (audio_output_) {
        audio_output_->setVolume(static_cast<float>(volume_0_100) / 100.0f);
    }
}

void PipelineController::set_muted(bool muted) {
    if (audio_output_) {
        audio_output_->setMuted(muted);
    }
}

void PipelineController::set_playback_rate(double rate) {
    if (worker_) worker_->set_speed(rate);
    if (audio_player_) audio_player_->setPlaybackRate(rate);
}

void PipelineController::on_worker_loop_restarted() {
    if (audio_player_ && !is_live_) {
        audio_player_->setPosition(0);
        audio_player_->play();
    }
}

#include "PipelineController.moc"
