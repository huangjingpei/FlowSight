#pragma once

#include <QWidget>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVideoSink>
#include <QVideoFrame>
#include <QPainter>
#include <QImage>
#include <QUrl>
#include <QHideEvent>
#include "PlaybackControlBar.hpp"

/**
 * @brief 纯 Qt 视频呈现画布 (彻底杜绝 Win32 HWND 穿透与页面切换滞留残影)
 */
class VideoCanvasWidget : public QWidget {
    Q_OBJECT
public:
    explicit VideoCanvasWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setAttribute(Qt::WA_OpaquePaintEvent, false);
        sink_ = new QVideoSink(this);
        connect(sink_, &QVideoSink::videoFrameChanged, this, &VideoCanvasWidget::on_video_frame_changed);
    }

    QVideoSink* video_sink() const { return sink_; }

    void clear() {
        current_image_ = QImage();
        update();
    }

    const QImage& current_image() const { return current_image_; }

protected:
    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);

        QRect r = rect();
        p.setPen(QColor("#21262d"));
        p.setBrush(QColor("#0d1117"));
        p.drawRoundedRect(r.adjusted(0, 0, -1, -1), 6, 6);

        if (!current_image_.isNull()) {
            QSize target_size = current_image_.size().scaled(std::max(1, r.width() - 4), std::max(1, r.height() - 4), Qt::KeepAspectRatio);
            QRect target_rect(
                (width() - target_size.width()) / 2,
                (height() - target_size.height()) / 2,
                target_size.width(),
                target_size.height()
            );
            p.drawImage(target_rect, current_image_);
        } else {
            p.setPen(QColor("#8b949e"));
            QFont f = p.font();
            f.setPointSize(10);
            p.setFont(f);
            p.drawText(r, Qt::AlignCenter, "🎬 证据切片播放器 (双击左侧通行流水起播)");
        }
    }

private slots:
    void on_video_frame_changed(const QVideoFrame& frame) {
        if (frame.isValid()) {
            current_image_ = frame.toImage();
        } else {
            current_image_ = QImage();
        }
        update();
    }

private:
    QVideoSink* sink_ = nullptr;
    QImage current_image_;
};

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
    void hideEvent(QHideEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void on_media_status_changed(QMediaPlayer::MediaStatus status);
    void on_player_playback_state_changed(QMediaPlayer::PlaybackState state);

private:
    void setup_ui();

    // 核心音视频解码与渲染引擎
    QMediaPlayer* player_ = nullptr;
    QAudioOutput* audio_output_ = nullptr;
    VideoCanvasWidget* video_canvas_ = nullptr;

    // 工业级标准首尾时间控制条
    PlaybackControlBar* control_bar_ = nullptr;

    // 内部运行状态
    QString current_path_;
    bool is_loop_enabled_ = false;
    bool is_fullscreen_ = false;
};
