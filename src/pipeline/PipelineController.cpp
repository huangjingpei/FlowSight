#include "PipelineController.hpp"
#include "ai/OnnxDetector.hpp"
#include "ai/AttributeClassifier.hpp"
#include "ai/VehicleClassifier.hpp"
#include "tracking/ByteTrackWrapper.hpp"
#include "analytics/RoiCounter.hpp"
#include "storage/DatabaseManager.hpp"
#include "storage/ConfigManager.hpp"
#include "video/ClipRecorder.hpp"
#include "network/EdgeReportManager.hpp"

#include <opencv2/opencv.hpp>
#include <QDateTime>
#include <QElapsedTimer>
#include <QDebug>
#include <QCoreApplication>
#include <QApplication>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QTextStream>
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

    // 1. 网络流 (RTSP / RTMP / HTTP) - 配置 TCP 传输与 2 秒超时快速熔断
    if (path_str.find("://") != std::string::npos) {
#if defined(_WIN32)
        _putenv("OPENCV_FFMPEG_CAPTURE_OPTIONS=rtsp_transport;tcp|stimeout;2000000|max_delay;500000");
#else
        setenv("OPENCV_FFMPEG_CAPTURE_OPTIONS", "rtsp_transport;tcp|stimeout;2000000|max_delay;500000", 1);
#endif
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

    std::mutex biz_mtx_;
    std::string pending_biz_type_;
    std::atomic<bool> biz_type_changed_{false};

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

    // RTSP 实时流异步拉流抓帧线程 (解耦拉流与推理，杜绝网络延迟累积)
    std::thread grabber_thread_;
    std::atomic<bool> grabber_running_{false};
    std::mutex live_frame_mtx_;
    cv::Mat live_latest_frame_;
    std::atomic<int64_t> live_frame_seq_{0};
    int64_t last_consumed_seq_{0};

    // 核心拉流与视频捕捉句柄及线程安全保护锁
    cv::VideoCapture cap_;
    std::mutex cap_mtx_;

    ~Worker() override {
        stop();
    }

    void stop_grabber() {
        if (grabber_running_.load()) {
            grabber_running_.store(false);
            if (grabber_thread_.joinable()) {
                grabber_thread_.join();
            }
        }
    }

    void stop() {
        is_running_.store(false);
        is_paused_.store(false);
        stop_grabber();
        {
            std::lock_guard<std::mutex> lk(cap_mtx_);
            if (cap_.isOpened()) {
                cap_.release();
            }
        }
        if (clip_recorder_) {
            clip_recorder_->stop();
        }
    }

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

    void set_flow_direction(float angle_deg, bool invert) {
        std::lock_guard<std::mutex> lock(roi_mtx_);
        for (auto& r : active_rois_) {
            r.flow_angle_deg = angle_deg;
            r.invert_direction = invert;
        }
        if (!active_rois_.empty()) {
            active_roi_.flow_angle_deg = angle_deg;
            active_roi_.invert_direction = invert;
        }
        if (roi_counter_) {
            roi_counter_->set_roi_configs(active_rois_);
        }
    }

    void change_video_source(const std::string& new_source) {
        std::lock_guard<std::mutex> lock(source_mtx_);
        pending_video_source_ = new_source;
        source_changed_ = true;
    }

    void change_biz_type(const std::string& new_biz) {
        std::lock_guard<std::mutex> lock(biz_mtx_);
        pending_biz_type_ = new_biz;
        biz_type_changed_ = true;
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

    void reset_today_counts() {
        today_entry_ = 0;
        today_exit_ = 0;
        if (roi_counter_) {
            roi_counter_->reset();
        }
        if (tracker_) {
            tracker_->reset();
        }
        if (ConfigManager::instance().is_vehicle_mode()) {
            VehicleClassifier::instance().clear_cache();
        } else {
            AttributeClassifier::instance().clear_cache();
        }
        StatisticsSnapshot snap;
        snap.current_occupancy = 0;
        snap.today_entry = 0;
        snap.today_exit = 0;
        snap.today_total = 0;
        if (detector_) snap.backend_name = detector_->get_backend_name();
        emit stats_updated(snap);
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

        // 智能双流感知：依据配置按需加载模型 (二选一模式，严格防范边缘内存 OOM)
        bool is_veh_mode = ConfigManager::instance().is_vehicle_mode();
        if (tracker_) {
            tracker_->set_vehicle_mode(is_veh_mode);
        }
        if (is_veh_mode) {
            detector_->set_biz_type("VEHICLE");
            AttributeClassifier::instance().stop();
            VehicleClassifier::instance().init();
            qInfo() << "[PipelineController] 智能感知流水线路由初始化: 【车流感知模式】(机动车检测 + LPR 车牌识别 + 车型颜色分析)";
        } else {
            detector_->set_biz_type("PEDESTRIAN");
            VehicleClassifier::instance().stop();
            std::string resolved_attr_model = resolve_resource_path("models/pedestrian_gender_int8.onnx");
            if (!QFileInfo::exists(QString::fromStdString(resolved_attr_model))) {
                resolved_attr_model = resolve_resource_path("models/mobilenetv3_attributes.onnx");
            }
            AttributeClassifier::instance().init(resolved_attr_model);
            qInfo() << "[PipelineController] 智能感知流水线路由初始化: 【客流感知模式】(行人检测 + 性别年龄深度分析)";
        }

        // 开机恢复今日已入库的进出累计客流数并立即广播同步大屏
        DatabaseManager::instance().get_today_counts(today_entry_, today_exit_);
        {
            StatisticsSnapshot init_snap;
            init_snap.current_occupancy = 0;
            init_snap.today_entry = today_entry_;
            init_snap.today_exit = today_exit_;
            init_snap.today_total = today_entry_ + today_exit_;
            init_snap.biz_type = is_veh_mode ? "VEHICLE" : "PEDESTRIAN";
            if (detector_) init_snap.backend_name = detector_->get_backend_name();
            emit stats_updated(init_snap);
        }

        std::string cur_source = video_source_;
        std::string resolved_video = resolve_resource_path(cur_source);
        bool has_real_video = false;
        double video_fps = 30.0;
        double frame_count = 0.0;
        bool is_live = false;
        qint64 total_duration_ms = 0;

        auto init_capture = [&](const std::string& source_path) {
            stop_grabber();
            {
                std::lock_guard<std::mutex> lk(cap_mtx_);
                if (cap_.isOpened()) cap_.release();
                resolved_video = resolve_resource_path(source_path);
                has_real_video = open_video_capture(cap_, resolved_video);

                if (has_real_video) {
                    video_fps = cap_.get(cv::CAP_PROP_FPS);
                    if (video_fps <= 0.0 || video_fps > 120.0) video_fps = 30.0;
                    frame_count = cap_.get(cv::CAP_PROP_FRAME_COUNT);
                }
            }

            if (has_real_video) {
                is_live = (source_path.find("rtsp://") != std::string::npos ||
                           source_path.find("rtmp://") != std::string::npos ||
                           frame_count <= 0);

                total_duration_ms = is_live ? 0 : static_cast<qint64>((frame_count / video_fps) * 1000.0);
                emit media_info_ready(is_live, total_duration_ms, video_fps);

                // 针对网络实时流启动异步专用抓帧线程，始终抓取最新帧丢弃旧帧，内置断网自愈重连状态机
                if (is_live) {
                    live_frame_seq_ = 0;
                    last_consumed_seq_ = 0;
                    grabber_running_ = true;
                    grabber_thread_ = std::thread([this, resolved_video]() {
                        cv::Mat tmp_frame;
                        int failed_reads = 0;
                        int backoff_ms = 1000;
                        while (grabber_running_.load() && is_running_.load()) {
                            bool read_ok = false;
                            {
                                std::lock_guard<std::mutex> lk(cap_mtx_);
                                if (cap_.isOpened()) {
                                    read_ok = cap_.read(tmp_frame);
                                }
                            }
                            if (!read_ok || tmp_frame.empty()) {
                                failed_reads++;
                                // 连续读失败达 40 次（约 2 秒读不到有效帧，判定断流）
                                if (failed_reads >= 40) {
                                    qWarning() << "[PipelineController] RTSP 视频流断开，正在尝试自愈重连...";
                                    emit network_state_changed(false, QString("实时流断开，正在重连 (等待 %1s)...").arg(backoff_ms / 1000));
                                    {
                                        std::lock_guard<std::mutex> lk(cap_mtx_);
                                        if (cap_.isOpened()) cap_.release();
                                    }

                                    for (int s = 0; s < backoff_ms; s += 100) {
                                        if (!grabber_running_.load() || !is_running_.load()) break;
                                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                                    }
                                    if (!grabber_running_.load() || !is_running_.load()) break;

                                    bool reopened = false;
                                    {
                                        std::lock_guard<std::mutex> lk(cap_mtx_);
                                        reopened = open_video_capture(cap_, resolved_video);
                                    }
                                    if (reopened) {
                                        qInfo() << "[PipelineController] RTSP 实时流重连成功！";
                                        emit network_state_changed(true, "实时流已连接");
                                        failed_reads = 0;
                                        backoff_ms = 1000;
                                    } else {
                                        backoff_ms = std::min(backoff_ms * 2, 10000); // 指数退避，最高 10 秒
                                    }
                                } else {
                                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                                }
                                continue;
                            }

                            if (failed_reads > 0) {
                                failed_reads = 0;
                                backoff_ms = 1000;
                                emit network_state_changed(true, "实时流已连接");
                            }

                            {
                                std::lock_guard<std::mutex> lk(live_frame_mtx_);
                                live_latest_frame_ = tmp_frame;
                                live_frame_seq_++;
                            }
                            std::this_thread::sleep_for(std::chrono::milliseconds(2));
                        }
                    });
                }
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
                last_w_ = 0;
                last_h_ = 0; // 重置宽高缓存，强制在新视频首帧自适应分辨率防区
                init_capture(cur_source);
                if (tracker_) tracker_->reset();
                if (roi_counter_) roi_counter_->reset();
                if (ConfigManager::instance().is_vehicle_mode()) {
                    VehicleClassifier::instance().clear_cache();
                } else {
                    AttributeClassifier::instance().clear_cache();
                }
                // 重新从 SQLite 恢复今日最新累计客流，绝不暴力清零！
                DatabaseManager::instance().get_today_counts(today_entry_, today_exit_);
                StatisticsSnapshot snap;
                snap.current_occupancy = 0;
                snap.today_entry = today_entry_;
                snap.today_exit = today_exit_;
                snap.today_total = today_entry_ + today_exit_;
                if (detector_) snap.backend_name = detector_->get_backend_name();
                emit stats_updated(snap);
            }

            // 1.1 动态检测业务感知模式热切换 (客流 <-> 车流 智能二选一热路由)
            bool trigger_biz_switch = false;
            std::string target_biz;
            if (biz_type_changed_.exchange(false)) {
                std::lock_guard<std::mutex> lock(biz_mtx_);
                target_biz = pending_biz_type_;
                trigger_biz_switch = true;
            } else {
                bool cur_cfg_veh = ConfigManager::instance().is_vehicle_mode();
                if (cur_cfg_veh != is_veh_mode) {
                    target_biz = cur_cfg_veh ? "VEHICLE" : "PEDESTRIAN";
                    trigger_biz_switch = true;
                }
            }

            if (trigger_biz_switch && !target_biz.empty()) {
                bool next_veh_mode = (target_biz == "VEHICLE");
                if (next_veh_mode != is_veh_mode) {
                    is_veh_mode = next_veh_mode;
                    if (tracker_) {
                        tracker_->set_vehicle_mode(is_veh_mode);
                        tracker_->reset();
                    }
                    if (roi_counter_) roi_counter_->reset();
                    if (detector_) {
                        detector_->set_biz_type(is_veh_mode ? "VEHICLE" : "PEDESTRIAN");
                    }
                    if (is_veh_mode) {
                        AttributeClassifier::instance().stop();
                        VehicleClassifier::instance().init();
                        qInfo() << "[PipelineController] 智能感知模式热切换 => 【车流感知模式】(机动车检测 + LPR 车牌识别 + 车型颜色分析)";
                    } else {
                        VehicleClassifier::instance().stop();
                        std::string resolved_attr_model = resolve_resource_path("models/pedestrian_gender_int8.onnx");
                        if (!QFileInfo::exists(QString::fromStdString(resolved_attr_model))) {
                            resolved_attr_model = resolve_resource_path("models/mobilenetv3_attributes.onnx");
                        }
                        AttributeClassifier::instance().init(resolved_attr_model);
                        qInfo() << "[PipelineController] 智能感知模式热切换 => 【客流感知模式】(行人检测 + 性别年龄分析)";
                    }
                    StatisticsSnapshot snap;
                    snap.current_occupancy = 0;
                    snap.today_entry = today_entry_;
                    snap.today_exit = today_exit_;
                    snap.today_total = today_entry_ + today_exit_;
                    snap.biz_type = is_veh_mode ? "VEHICLE" : "PEDESTRIAN";
                    if (detector_) snap.backend_name = detector_->get_backend_name();
                    emit stats_updated(snap);
                }
            }

            // 2. 暂停与单帧步进处理
            if (is_paused_) {
                int step = step_requested_.exchange(0);
                if (step != 0 && has_real_video) {
                    if (step < 0) {
                        if (tracker_) tracker_->reset();
                        if (roi_counter_) roi_counter_->reset();
                    }
                    std::lock_guard<std::mutex> lk(cap_mtx_);
                    if (cap_.isOpened()) {
                        double cur_frame = cap_.get(cv::CAP_PROP_POS_FRAMES);
                        double target_frame = cur_frame + (step > 0 ? 1 : -2);
                        if (target_frame < 0) target_frame = 0;
                        cap_.set(cv::CAP_PROP_POS_FRAMES, target_frame);
                        if (cap_.read(frame) && !frame.empty()) {
                            qint64 pos_ms = static_cast<qint64>(cap_.get(cv::CAP_PROP_POS_MSEC));
                            emit position_changed(pos_ms);

                            // 运行单帧分析并投递
                            std::vector<Detection> detections;
                            if (model_loaded) detector_->detect(frame, detections);
                            auto tracks = tracker_->update(detections);
                            QImage qimg(frame.data, frame.cols, frame.rows, static_cast<int>(frame.step), QImage::Format_BGR888);
                            emit frame_ready(qimg.copy(), tracks);
                        }
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
                    std::lock_guard<std::mutex> lk(cap_mtx_);
                    if (cap_.isOpened()) {
                        double target_frame = (target_ms * video_fps) / 1000.0;
                        if (target_frame < 0) target_frame = 0;
                        if (frame_count > 0 && target_frame >= frame_count) target_frame = frame_count - 1;
                        cap_.set(cv::CAP_PROP_POS_FRAMES, target_frame);
                    }
                }
                // 时间轴拖拽定位后，必须重置跟踪状态机、ROI位移上下文与切片环形缓冲
                // 彻底杜绝拖拽引起的跨时空伪位移误触发与切片错乱
                if (tracker_) tracker_->reset();
                if (roi_counter_) roi_counter_->reset();
                if (ConfigManager::instance().is_vehicle_mode()) {
                    VehicleClassifier::instance().clear_cache();
                } else {
                    AttributeClassifier::instance().clear_cache();
                }
                if (clip_recorder_) clip_recorder_->clear();
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

            // 4. 读取视频帧 (实时流通过异步抓帧解耦消费，本地流精确步调直读)
            if (is_live) {
                int64_t cur_seq = live_frame_seq_.load();
                if (cur_seq == last_consumed_seq_) {
                    QThread::msleep(5);
                    continue;
                }
                {
                    std::lock_guard<std::mutex> lk(live_frame_mtx_);
                    if (live_latest_frame_.empty()) {
                        QThread::msleep(10);
                        continue;
                    }
                    frame = live_latest_frame_.clone();
                    last_consumed_seq_ = cur_seq;
                }
            } else {
                bool read_ok = false;
                {
                    std::lock_guard<std::mutex> lk(cap_mtx_);
                    if (cap_.isOpened()) {
                        read_ok = cap_.read(frame);
                    }
                }
                if (!read_ok || frame.empty()) {
                    // 本地视频循环播放
                    {
                        std::lock_guard<std::mutex> lk(cap_mtx_);
                        if (cap_.isOpened()) {
                            cap_.set(cv::CAP_PROP_POS_FRAMES, 0);
                        }
                    }
                    if (tracker_) tracker_->reset();
                    if (roi_counter_) roi_counter_->reset();
                    if (ConfigManager::instance().is_vehicle_mode()) {
                        VehicleClassifier::instance().clear_cache();
                    } else {
                        AttributeClassifier::instance().clear_cache();
                    }
                    emit loop_restarted();
                    bool second_read_ok = false;
                    {
                        std::lock_guard<std::mutex> lk(cap_mtx_);
                        if (cap_.isOpened()) {
                            second_read_ok = cap_.read(frame);
                        }
                    }
                    if (!second_read_ok || frame.empty()) {
                        QThread::msleep(40);
                        continue;
                    }
                }
                {
                    std::lock_guard<std::mutex> lk(cap_mtx_);
                    if (cap_.isOpened()) {
                        qint64 cur_pos_ms = static_cast<qint64>(cap_.get(cv::CAP_PROP_POS_MSEC));
                        emit position_changed(cur_pos_ms);
                    }
                }
            }

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

            // 6.1 针对活跃目标框触发二级异步属性与车牌分类识别 (按需路由)
            if (is_veh_mode) {
                for (auto& trk : tracks) {
                    int x = std::clamp(static_cast<int>(trk.bbox.x), 0, frame.cols - 1);
                    int y = std::clamp(static_cast<int>(trk.bbox.y), 0, frame.rows - 1);
                    int w = std::clamp(static_cast<int>(trk.bbox.width), 1, frame.cols - x);
                    int h = std::clamp(static_cast<int>(trk.bbox.height), 1, frame.rows - y);
                    if (w >= 30 && h >= 30) {
                        cv::Rect crop_rect(x, y, w, h);
                        cv::Mat crop = frame(crop_rect);
                        VehicleClassifier::instance().classify_async(trk.track_id, 2 /* car */, crop);
                    }
                    VehicleInfo v_info;
                    if (VehicleClassifier::instance().get_vehicle_info(trk.track_id, v_info)) {
                        trk.plate_number = v_info.plate_number;
                        trk.plate_color = v_info.plate_color;
                        trk.vehicle_type = v_info.vehicle_type;
                        trk.vehicle_color = v_info.vehicle_color;
                        if (!trk.plate_number.empty()) {
                            DatabaseManager::instance().update_event_vehicle_attributes(trk.track_id, trk.plate_number, trk.plate_color, trk.vehicle_type, trk.vehicle_color);
                            EdgeReportManager::instance().update_event_vehicle_attributes(trk.track_id, trk.plate_number, trk.plate_color, trk.vehicle_type, trk.vehicle_color);
                            emit event_vehicle_attributes_updated(trk.track_id, QString::fromStdString(trk.plate_number), QString::fromStdString(trk.plate_color), QString::fromStdString(trk.vehicle_type), QString::fromStdString(trk.vehicle_color));
                        }
                    }
                }
            } else {
                for (auto& trk : tracks) {
                    int x = std::clamp(static_cast<int>(trk.bbox.x), 0, frame.cols - 1);
                    int y = std::clamp(static_cast<int>(trk.bbox.y), 0, frame.rows - 1);
                    int w = std::clamp(static_cast<int>(trk.bbox.width), 1, frame.cols - x);
                    int h = std::clamp(static_cast<int>(trk.bbox.height), 1, frame.rows - y);
                    if (w >= 28 && h >= 56) {
                        cv::Rect crop_rect(x, y, w, h);
                        cv::Mat crop = frame(crop_rect);
                        AttributeClassifier::instance().classify_async(trk.track_id, crop);
                    }
                    if (AttributeClassifier::instance().get_attributes(trk.track_id, trk.gender, trk.age_group)) {
                        // 若已成功获得真实识别属性，尝试异步校准该目标若在早期被记为 '未知' 的历史通行事件
                        if (trk.gender != "未知") {
                            DatabaseManager::instance().update_event_attributes(trk.track_id, trk.gender, trk.age_group);
                            EdgeReportManager::instance().update_event_attributes(trk.track_id, trk.gender, trk.age_group);
                            emit event_attributes_updated(trk.track_id, QString::fromStdString(trk.gender), QString::fromStdString(trk.age_group));
                        }
                    }
                }
            }

            // 7. 区域滞后防抖计数
            int current_occupancy = 0;
            std::vector<PassageEvent> events;
            roi_counter_->process_frame(tracks, now_ms, current_occupancy, events, frame.cols, frame.rows);

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

                // 严格以摄像头/视频源原生帧率 (video_fps) 编码，杜绝因 CPU 推理快慢导致的时间尺度失真
                int clip_fps = (video_fps >= 10.0 && video_fps <= 60.0) ? static_cast<int>(std::round(video_fps)) : 25;
                std::string clip = clip_recorder_->save_clip_async("clips", ev.track_id, ev.timestamp_ms, clip_fps);
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
                snap.biz_type = is_veh_mode ? "VEHICLE" : "PEDESTRIAN";
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
                snap.biz_type = is_veh_mode ? "VEHICLE" : "PEDESTRIAN";
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

        stop_grabber();
        {
            std::lock_guard<std::mutex> lk(cap_mtx_);
            if (cap_.isOpened()) cap_.release();
        }
    }

signals:
    void frame_ready(const QImage& frame, const std::vector<TrackObject>& tracks);
    void stats_updated(const StatisticsSnapshot& stats);
    void event_occurred(const PassageEvent& event);
    void event_attributes_updated(int track_id, const QString& gender, const QString& age_group);
    void event_vehicle_attributes_updated(int track_id, const QString& plate_number, const QString& plate_color, const QString& vehicle_type, const QString& vehicle_color);
    void media_info_ready(bool is_live, qint64 duration_ms, double fps);
    void position_changed(qint64 position_ms);
    void loop_restarted();
    void crowd_surge_status_changed(bool is_surged, int current_occupancy, int threshold);
    void roi_auto_initialized(const RoiConfig& config);
    void rois_auto_initialized(const std::vector<RoiConfig>& configs);
    void network_state_changed(bool is_connected, const QString& status_msg);
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
    connect(worker_.get(), &Worker::event_attributes_updated, this, &PipelineController::event_attributes_updated);
    connect(worker_.get(), &Worker::event_vehicle_attributes_updated, this, &PipelineController::event_vehicle_attributes_updated);
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
    connect(worker_.get(), &Worker::network_state_changed, this, &PipelineController::network_state_changed);

    worker_thread_.start();

    // 启动初始媒体音频 (若为本地视频文件)
    set_video_source(video_source);
}

void PipelineController::stop_pipeline() {
    if (audio_player_) {
        audio_player_->stop();
    }
    if (worker_) {
        worker_->stop();
        worker_thread_.quit();
        if (!worker_thread_.wait(3000)) {
            qWarning() << "[PipelineController] Worker 线程未在 3 秒内退出，强制终止...";
            worker_thread_.terminate();
            worker_thread_.wait(1000);
        }
        worker_.reset();
    }
}

void PipelineController::set_video_source(const std::string& video_source) {
    if (worker_) {
        if (worker_->video_source_ != video_source) {
            worker_->change_video_source(video_source);
        }
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

void PipelineController::set_biz_type(const std::string& biz_type) {
    if (worker_) {
        worker_->change_biz_type(biz_type);
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

void PipelineController::reset_today_counts() {
    if (worker_) {
        worker_->reset_today_counts();
    }
}

void PipelineController::set_roi_flow_direction(float angle_deg, bool invert) {
    if (worker_) {
        worker_->set_flow_direction(angle_deg, invert);
    }
}

#include "PipelineController.moc"
