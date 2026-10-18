#pragma once

#include <QObject>
#include <QThread>
#include <QImage>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <memory>
#include <string>
#include "common/Types.hpp"

class PipelineController : public QObject {
    Q_OBJECT
public:
    explicit PipelineController(QObject* parent = nullptr);
    ~PipelineController() override;

    void start_pipeline(const std::string& model_path, const std::string& video_source);
    void stop_pipeline();
    void set_video_source(const std::string& video_source);
    void update_roi_config(const RoiConfig& config);
    void update_roi_configs(const std::vector<RoiConfig>& configs);
    RoiConfig get_current_roi() const;
    std::vector<RoiConfig> get_current_rois() const;

    // 专业音视频播放控制 (支持音画同步与精确进度跳转)
    void play();
    void pause();
    void stop();
    void seek(qint64 position_ms);
    void step_forward(int ms = 33);
    void step_backward(int ms = 33);
    void set_volume(int volume_0_100);
    void set_muted(bool muted);
    void set_playback_rate(double rate);

signals:
    void frame_ready(const QImage& frame, const std::vector<TrackObject>& tracks);
    void stats_updated(const StatisticsSnapshot& stats);
    void event_occurred(const PassageEvent& event);

    // 多媒体时钟与状态信号 (向 UI 广播首尾时间与模式自适应)
    void media_info_ready(bool is_live, qint64 duration_ms, double fps);
    void position_changed(qint64 position_ms);
    void playback_state_changed(bool is_playing);

    // 区域人员超载与拥挤度风控告警信号
    void crowd_surge_status_changed(bool is_surged, int current_occupancy, int threshold);

    // ROI 自动就绪通知 (用于开箱即用自动适配视频分辨率防区)
    void roi_auto_initialized(const RoiConfig& config);
    void rois_auto_initialized(const std::vector<RoiConfig>& configs);

private slots:
    void on_worker_loop_restarted();

private:
    class Worker;
    std::unique_ptr<Worker> worker_;
    QThread worker_thread_;

    // MP4 音频输出与声画对齐驱动引擎
    QMediaPlayer* audio_player_ = nullptr;
    QAudioOutput* audio_output_ = nullptr;
    bool is_live_ = false;
    qint64 duration_ms_ = 0;
};
