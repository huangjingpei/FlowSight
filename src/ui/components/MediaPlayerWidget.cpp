#include "MediaPlayerWidget.hpp"
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMessageBox>
#include <QToolTip>

MediaPlayerWidget::MediaPlayerWidget(QWidget* parent) : QWidget(parent) {
    setup_ui();
}

MediaPlayerWidget::~MediaPlayerWidget() {
    if (player_) {
        player_->stop();
    }
}

void MediaPlayerWidget::setup_ui() {
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(6);

    // 1. 核心音视频输出对象
    player_ = new QMediaPlayer(this);
    audio_output_ = new QAudioOutput(this);
    audio_output_->setVolume(0.8f);
    player_->setAudioOutput(audio_output_);

    video_widget_ = new QVideoWidget(this);
    video_widget_->setStyleSheet("background-color: #0d1117; border: 1px solid #21262d; border-radius: 6px;");
    video_widget_->setMinimumHeight(240);
    video_widget_->installEventFilter(this);
    player_->setVideoOutput(video_widget_);
    main_layout->addWidget(video_widget_, 1);

    // 2. 挂载标准首尾时间播放控制条
    control_bar_ = new PlaybackControlBar(this);
    control_bar_->set_volume(80);
    main_layout->addWidget(control_bar_);

    // 3. 信号与槽连接
    connect(control_bar_, &PlaybackControlBar::play_requested, this, &MediaPlayerWidget::play);
    connect(control_bar_, &PlaybackControlBar::pause_requested, this, &MediaPlayerWidget::pause);
    connect(control_bar_, &PlaybackControlBar::stop_requested, this, &MediaPlayerWidget::stop);
    connect(control_bar_, &PlaybackControlBar::step_backward_requested, this, &MediaPlayerWidget::step_backward);
    connect(control_bar_, &PlaybackControlBar::step_forward_requested, this, &MediaPlayerWidget::step_forward);
    connect(control_bar_, &PlaybackControlBar::seek_requested, this, &MediaPlayerWidget::seek);
    connect(control_bar_, &PlaybackControlBar::loop_toggled, this, &MediaPlayerWidget::set_loop);
    connect(control_bar_, &PlaybackControlBar::volume_changed, this, &MediaPlayerWidget::set_volume);
    connect(control_bar_, &PlaybackControlBar::mute_toggled, audio_output_, &QAudioOutput::setMuted);
    connect(control_bar_, &PlaybackControlBar::rate_changed, this, &MediaPlayerWidget::set_playback_rate);
    connect(control_bar_, &PlaybackControlBar::snapshot_requested, this, &MediaPlayerWidget::take_snapshot);
    connect(control_bar_, &PlaybackControlBar::fullscreen_requested, this, &MediaPlayerWidget::toggle_fullscreen);

    // QMediaPlayer 事件同步到控制条
    connect(player_, &QMediaPlayer::durationChanged, this, [this](qint64 d) {
        control_bar_->set_duration(d);
        if (d > 0) {
            control_bar_->set_media_mode(false);
        }
    });
    connect(player_, &QMediaPlayer::positionChanged, this, [this](qint64 p) {
        control_bar_->set_position(p);
        emit position_changed(p);
    });
    connect(player_, &QMediaPlayer::mediaStatusChanged, this, &MediaPlayerWidget::on_media_status_changed);
    connect(player_, &QMediaPlayer::playbackStateChanged, this, &MediaPlayerWidget::on_player_playback_state_changed);
}

void MediaPlayerWidget::load_media(const QString& file_path_or_url) {
    if (file_path_or_url.isEmpty()) return;

    current_path_ = file_path_or_url;
    QUrl media_url;
    bool is_live = file_path_or_url.startsWith("rtsp://", Qt::CaseInsensitive) ||
                   file_path_or_url.startsWith("rtmp://", Qt::CaseInsensitive);

    if (file_path_or_url.contains("://")) {
        media_url = QUrl(file_path_or_url);
    } else {
        media_url = QUrl::fromLocalFile(file_path_or_url);
    }

    control_bar_->set_media_mode(is_live);
    player_->setSource(media_url);
    control_bar_->set_status_text("● 已载入", "#58a6ff");
    emit media_loaded(file_path_or_url, player_->duration());
}

void MediaPlayerWidget::play() {
    player_->play();
}

void MediaPlayerWidget::pause() {
    player_->pause();
}

void MediaPlayerWidget::stop() {
    player_->stop();
    control_bar_->set_position(0);
}

void MediaPlayerWidget::replay() {
    player_->setPosition(0);
    player_->play();
}

void MediaPlayerWidget::step_forward(int ms) {
    if (!player_) return;
    qint64 target = std::min(player_->duration(), player_->position() + ms);
    player_->setPosition(target);
}

void MediaPlayerWidget::step_backward(int ms) {
    if (!player_) return;
    qint64 target = std::max(qint64(0), player_->position() - ms);
    player_->setPosition(target);
}

void MediaPlayerWidget::seek(qint64 position_ms) {
    if (player_) {
        player_->setPosition(position_ms);
    }
}

void MediaPlayerWidget::set_loop(bool enable) {
    is_loop_enabled_ = enable;
    if (player_) {
        player_->setLoops(enable ? QMediaPlayer::Infinite : 1);
    }
}

void MediaPlayerWidget::set_volume(int volume_0_to_100) {
    if (audio_output_) {
        audio_output_->setVolume(static_cast<float>(volume_0_to_100) / 100.0f);
    }
}

void MediaPlayerWidget::set_playback_rate(qreal rate) {
    if (player_) {
        player_->setPlaybackRate(rate);
    }
}

qint64 MediaPlayerWidget::get_duration() const {
    return player_ ? player_->duration() : 0;
}

qint64 MediaPlayerWidget::get_position() const {
    return player_ ? player_->position() : 0;
}

bool MediaPlayerWidget::is_playing() const {
    return player_ && player_->playbackState() == QMediaPlayer::PlayingState;
}

bool MediaPlayerWidget::is_loop() const {
    return is_loop_enabled_;
}

QString MediaPlayerWidget::current_media_path() const {
    return current_path_;
}

void MediaPlayerWidget::take_snapshot() {
    if (!video_widget_) return;

    QDir().mkpath("snapshots");
    QString filename = QString("snapshots/snapshot_%1.png")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz"));

    QPixmap pixmap = video_widget_->grab();
    if (pixmap.isNull()) {
        QMessageBox::warning(this, "截图失败", "未能从播放器抓取到有效视频画面帧");
        return;
    }

    if (pixmap.save(filename, "PNG")) {
        emit snapshot_saved(filename);
        QToolTip::showText(mapToGlobal(QPoint(width() / 2, height() / 2)),
                           QString("✅ 抓拍成功已保存至:\n%1").arg(filename), this);
    } else {
        QMessageBox::warning(this, "保存失败", QString("无法写入快照到:\n%1").arg(filename));
    }
}

void MediaPlayerWidget::toggle_fullscreen() {
    if (!is_fullscreen_) {
        video_widget_->setParent(nullptr);
        video_widget_->setWindowFlags(Qt::Window);
        video_widget_->showFullScreen();
        is_fullscreen_ = true;
    } else {
        video_widget_->setWindowFlags(Qt::Widget);
        static_cast<QVBoxLayout*>(layout())->insertWidget(0, video_widget_, 1);
        video_widget_->showNormal();
        is_fullscreen_ = false;
    }
}

void MediaPlayerWidget::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape && is_fullscreen_) {
        toggle_fullscreen();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Space) {
        if (is_playing()) pause();
        else play();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Left) {
        step_backward(33);
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Right) {
        step_forward(33);
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

bool MediaPlayerWidget::eventFilter(QObject* watched, QEvent* event) {
    if (watched == video_widget_) {
        if (event->type() == QEvent::KeyPress) {
            auto* ke = static_cast<QKeyEvent*>(event);
            if (ke->key() == Qt::Key_Escape && is_fullscreen_) {
                toggle_fullscreen();
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonDblClick) {
            toggle_fullscreen();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void MediaPlayerWidget::on_media_status_changed(QMediaPlayer::MediaStatus status) {
    switch (status) {
        case QMediaPlayer::NoMedia:
            control_bar_->set_status_text("● 无媒体", "#8b949e");
            break;
        case QMediaPlayer::LoadingMedia:
            control_bar_->set_status_text("⏳ 载入中...", "#58a6ff");
            break;
        case QMediaPlayer::LoadedMedia:
            control_bar_->set_status_text("● 已就绪", "#3fb950");
            break;
        case QMediaPlayer::BufferingMedia:
            control_bar_->set_status_text("⏳ 缓冲中...", "#d2a8ff");
            break;
        case QMediaPlayer::BufferedMedia:
            control_bar_->set_status_text("● 播放中", "#3fb950");
            break;
        case QMediaPlayer::EndOfMedia:
            control_bar_->set_status_text("■ 播放结束", "#8b949e");
            if (is_loop_enabled_) replay();
            break;
        case QMediaPlayer::InvalidMedia:
            control_bar_->set_status_text("✖ 媒体格式无效", "#ff4757");
            break;
    }
}

void MediaPlayerWidget::on_player_playback_state_changed(QMediaPlayer::PlaybackState state) {
    control_bar_->set_playback_state(state == QMediaPlayer::PlayingState);
    emit playback_state_changed(state);
}
