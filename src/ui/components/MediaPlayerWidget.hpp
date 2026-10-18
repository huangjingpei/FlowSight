#pragma once

#include <QWidget>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVideoWidget>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QUrl>

/**
 * @brief 专业音视频通用播放器组件 (支持视频画面渲染与多通道音频播放)
 * @note  适用于实时视频流查看、安防溯源切片回放、单帧精细步进比对与关键帧证据抓拍。
 */
class MediaPlayerWidget : public QWidget {
    Q_OBJECT
public:
    explicit MediaPlayerWidget(QWidget* parent = nullptr);
    ~MediaPlayerWidget() override;

    // 基础播放控制 API
    void load_media(const QString& file_path_or_url);
    void play();
    void pause();
    void stop();
    void replay();

    // 步进与进度控制
    void step_forward(int ms = 33);   // 默认步进 1 帧 (30FPS 约为 33ms)
    void step_backward(int ms = 33);  // 默认步退 1 帧
    void seek(qint64 position_ms);

    // 播放属性配置
    void set_loop(bool enable);
    void set_volume(int volume_0_to_100);
    void set_playback_rate(qreal rate);

    // 状态查询
    qint64 get_duration() const;
    qint64 get_position() const;
    bool is_playing() const;
    bool is_loop() const;
    QString current_media_path() const;

signals:
    void media_loaded(const QString& media_path, qint64 duration_ms);
    void playback_state_changed(QMediaPlayer::PlaybackState state);
    void position_changed(qint64 position_ms);
    void snapshot_saved(const QString& file_path);

public slots:
    void take_snapshot();
    void toggle_fullscreen();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void on_play_pause_clicked();
    void on_stop_clicked();
    void on_slider_moved(int position);
    void on_slider_pressed();
    void on_slider_released();
    void on_duration_changed(qint64 duration);
    void on_position_changed(qint64 position);
    void on_media_status_changed(QMediaPlayer::MediaStatus status);
    void on_playback_state_changed(QMediaPlayer::PlaybackState state);
    void on_volume_slider_changed(int value);
    void on_mute_clicked();
    void on_rate_changed(int index);

private:
    void setup_ui();
    void update_time_label(qint64 position, qint64 duration);
    static QString format_time(qint64 ms);

    // 核心音视频解码与渲染引擎
    QMediaPlayer* player_ = nullptr;
    QAudioOutput* audio_output_ = nullptr;
    QVideoWidget* video_widget_ = nullptr;

    // 播放控制栏控件
    QWidget* controls_container_ = nullptr;
    QPushButton* btn_play_pause_ = nullptr;
    QPushButton* btn_stop_ = nullptr;
    QPushButton* btn_step_back_ = nullptr;
    QPushButton* btn_step_fwd_ = nullptr;
    QPushButton* btn_loop_ = nullptr;
    QPushButton* btn_snapshot_ = nullptr;
    QPushButton* btn_fullscreen_ = nullptr;

    QSlider* progress_slider_ = nullptr;
    QLabel* time_label_ = nullptr;

    QPushButton* btn_mute_ = nullptr;
    QSlider* volume_slider_ = nullptr;
    QComboBox* rate_combo_ = nullptr;

    QLabel* status_tag_ = nullptr;

    // 内部运行状态
    QString current_path_;
    bool is_slider_dragging_ = false;
    bool is_loop_enabled_ = false;
    int saved_volume_ = 80;
    bool is_fullscreen_ = false;
};
