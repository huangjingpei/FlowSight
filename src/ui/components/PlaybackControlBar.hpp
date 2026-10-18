#pragma once

#include <QWidget>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

/**
 * @brief 专业音视频通用播放控制条 (符合工业安防与主流播放器标准)
 * @note  时间轴采用标准排版: [当前首部时间 (00:00)] ━━━━●━━━━ [媒体总长尾部时间 (00:09)]
 *        支持点播模式 (MP4/MKV) 与实时直播流模式 (RTSP/RTMP) 智能动态自适应。
 */
class PlaybackControlBar : public QWidget {
    Q_OBJECT
public:
    explicit PlaybackControlBar(QWidget* parent = nullptr);
    ~PlaybackControlBar() override = default;

    // 模式切换
    void set_media_mode(bool is_live_stream);
    bool is_live_stream() const { return is_live_; }

    // 状态配置
    void set_duration(qint64 duration_ms);
    void set_position(qint64 position_ms);
    void set_playback_state(bool is_playing);
    void set_volume(int volume_0_100);
    void set_muted(bool muted);
    void set_status_text(const QString& text, const QString& color_hex = "#3fb950");

    qint64 get_duration() const { return duration_ms_; }
    qint64 get_position() const { return position_ms_; }
    bool is_playing() const { return is_playing_; }

signals:
    void play_requested();
    void pause_requested();
    void stop_requested();
    void seek_requested(qint64 position_ms);
    void step_forward_requested(int ms);
    void step_backward_requested(int ms);
    void volume_changed(int volume);
    void mute_toggled(bool muted);
    void rate_changed(qreal rate);
    void loop_toggled(bool loop);
    void snapshot_requested();
    void fullscreen_requested();

private slots:
    void on_play_pause_clicked();
    void on_slider_moved(int value);
    void on_slider_pressed();
    void on_slider_released();
    void on_mute_clicked();
    void on_volume_slider_changed(int value);
    void on_rate_combo_changed(int index);

private:
    void setup_ui();
    static QString format_time(qint64 ms);

    // 1. 时间轴控件 (首尾时间标准布局)
    QLabel* lbl_current_time_ = nullptr;
    QSlider* slider_progress_ = nullptr;
    QLabel* lbl_total_time_ = nullptr;

    // 2. 核心播放控制
    QPushButton* btn_play_pause_ = nullptr;
    QPushButton* btn_stop_ = nullptr;
    QPushButton* btn_step_back_ = nullptr;
    QPushButton* btn_step_fwd_ = nullptr;
    QLabel* status_badge_ = nullptr;

    // 3. 音频与扩展控制
    QPushButton* btn_mute_ = nullptr;
    QSlider* slider_volume_ = nullptr;
    QLabel* lbl_volume_val_ = nullptr;
    QComboBox* combo_rate_ = nullptr;
    QPushButton* btn_loop_ = nullptr;
    QPushButton* btn_snapshot_ = nullptr;
    QPushButton* btn_fullscreen_ = nullptr;

    // 内部运行状态
    bool is_live_ = false;
    bool is_playing_ = false;
    bool is_muted_ = false;
    bool is_slider_dragging_ = false;
    qint64 duration_ms_ = 0;
    qint64 position_ms_ = 0;
    int saved_volume_ = 80;
};
