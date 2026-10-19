#pragma once
#include <QWidget>
#include <QLabel>
#include <QListWidget>
#include <QProgressBar>
#include "common/Types.hpp"
#include "ui/components/PlaybackControlBar.hpp"

class VideoCanvas;

class MonitorWidget : public QWidget {
    Q_OBJECT
public:
    explicit MonitorWidget(QWidget* parent = nullptr);

    void update_frame(const QImage& frame, const std::vector<TrackObject>& tracks);
    void update_stats(const StatisticsSnapshot& stats);
    void add_passage_event(const PassageEvent& ev);
    void update_active_roi(const RoiConfig& roi);
    void update_active_rois(const std::vector<RoiConfig>& rois);
    void set_crowd_surge_alert(bool is_surged, int current_occupancy, int threshold);

    // 播放控制条状态同步
    void set_media_info(bool is_live, qint64 duration_ms, double fps);
    void set_playback_position(qint64 pos_ms);
    void set_playback_state(bool is_playing);

    PlaybackControlBar* control_bar() const { return control_bar_; }

signals:
    void request_change_video(const QString& path);

public slots:
    void take_snapshot();
    void toggle_fullscreen();

private:
    void setup_ui();
    QWidget* create_nvr_kpi_card(const QString& card_id, const QString& title, const QString& init_val,
                                const QString& unit, const QString& accent_color, const QString& sub_info,
                                QLabel*& out_val_label, QLabel*& out_sub_label);

    VideoCanvas* canvas_ = nullptr;
    PlaybackControlBar* control_bar_ = nullptr;

    // 4 大核心数显指标标签
    QLabel* occupancy_label_ = nullptr;
    QLabel* occupancy_sub_label_ = nullptr;
    QLabel* entry_label_ = nullptr;
    QLabel* exit_label_ = nullptr;
    QLabel* total_label_ = nullptr;

    // NVR 通道状态与风控指示
    QLabel* channel_status_label_ = nullptr;
    QLabel* channel_info_label_ = nullptr;
    QLabel* res_tag_ = nullptr;

    QProgressBar* surge_progress_ = nullptr;
    QLabel* surge_text_label_ = nullptr;

    // 实时通行抓拍记录审计流
    QListWidget* ticker_list_ = nullptr;

    QWidget* video_container_ = nullptr;
    bool is_fullscreen_ = false;

    int current_occupancy_ = 0;
    int surge_threshold_ = 30;
};
