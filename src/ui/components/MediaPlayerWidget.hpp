#pragma once

#include <QWidget>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVideoWidget>
#include <QUrl>
#include "PlaybackControlBar.hpp"

/**
 * @brief 专业音视频通用播放器组件 (含声画同步解码输出与行业标准首尾时间控制条)
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
    void step_forward(int ms = 33);
    void step_backward(int ms = 33);
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
    void on_media_status_changed(QMediaPlayer::MediaStatus status);
    void on_player_playback_state_changed(QMediaPlayer::PlaybackState state);

private:
    void setup_ui();

    // 核心音视频解码与渲染引擎
    QMediaPlayer* player_ = nullptr;
    QAudioOutput* audio_output_ = nullptr;
    QVideoWidget* video_widget_ = nullptr;

    // 工业级标准首尾时间控制条
    PlaybackControlBar* control_bar_ = nullptr;

    // 内部运行状态
    QString current_path_;
    bool is_loop_enabled_ = false;
    bool is_fullscreen_ = false;
};
