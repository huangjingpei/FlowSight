#pragma once
#include <QWidget>
#include <QLabel>
#include <QListWidget>
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

    VideoCanvas* canvas_ = nullptr;
    PlaybackControlBar* control_bar_ = nullptr;

    QLabel* occupancy_label_ = nullptr;
    QLabel* entry_label_ = nullptr;
    QLabel* exit_label_ = nullptr;
    QLabel* total_label_ = nullptr;
    QLabel* res_tag_ = nullptr;
    QListWidget* ticker_list_ = nullptr;

    QWidget* video_container_ = nullptr;
    bool is_fullscreen_ = false;
};
