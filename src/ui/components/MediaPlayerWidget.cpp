#include "MediaPlayerWidget.hpp"
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMessageBox>
#include <QStyle>
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
    main_layout->setSpacing(4);

    // 1. 核心音视频输出对象
    player_ = new QMediaPlayer(this);
    audio_output_ = new QAudioOutput(this);
    audio_output_->setVolume(0.8f);
    player_->setAudioOutput(audio_output_);

    video_widget_ = new QVideoWidget(this);
    video_widget_->setStyleSheet("background-color: #0d1117; border: 1px solid #21262d; border-radius: 6px;");
    video_widget_->setMinimumHeight(240);
    player_->setVideoOutput(video_widget_);
    main_layout->addWidget(video_widget_, 1);

    // 2. 底部高科技控制条面板
    controls_container_ = new QWidget(this);
    controls_container_->setStyleSheet(
        "QWidget#controls_container { background-color: #161b22; border: 1px solid #30363d; border-radius: 6px; } "
        "QPushButton { background-color: #21262d; color: #e6edf3; border: 1px solid #30363d; border-radius: 4px; padding: 4px 8px; font-size: 12px; font-weight: 500; } "
        "QPushButton:hover { background-color: #30363d; border-color: #58a6ff; color: #58a6ff; } "
        "QPushButton:pressed { background-color: #1f6feb; color: #ffffff; } "
        "QComboBox { background-color: #21262d; color: #e6edf3; border: 1px solid #30363d; border-radius: 4px; padding: 2px 6px; font-size: 11px; } "
        "QComboBox QAbstractItemView { background-color: #161b22; color: #e6edf3; selection-background-color: #1f6feb; border: 1px solid #30363d; } "
        "QSlider::groove:horizontal { height: 4px; background: #21262d; border-radius: 2px; } "
        "QSlider::sub-page:horizontal { background: #58a6ff; border-radius: 2px; } "
        "QSlider::handle:horizontal { width: 12px; margin-top: -4px; margin-bottom: -4px; border-radius: 6px; background: #f0f6fc; } "
        "QSlider::handle:horizontal:hover { background: #58a6ff; }"
    );
    controls_container_->setObjectName("controls_container");
    auto* ctrl_vbox = new QVBoxLayout(controls_container_);
    ctrl_vbox->setContentsMargins(10, 8, 10, 8);
    ctrl_vbox->setSpacing(6);

    // 2.1 第一行: 进度条与实时时间戳
    auto* timeline_hbox = new QHBoxLayout();
    timeline_hbox->setContentsMargins(0, 0, 0, 0);
    timeline_hbox->setSpacing(10);

    progress_slider_ = new QSlider(Qt::Horizontal, controls_container_);
    progress_slider_->setRange(0, 0);
    progress_slider_->setCursor(Qt::PointingHandCursor);

    time_label_ = new QLabel("00:00 / 00:00", controls_container_);
    time_label_->setStyleSheet("color: #8b949e; font-family: 'Consolas', monospace; font-size: 12px;");

    status_tag_ = new QLabel("● 就绪", controls_container_);
    status_tag_->setStyleSheet("color: #3fb950; font-size: 11px; font-weight: bold;");

    timeline_hbox->addWidget(progress_slider_, 1);
    timeline_hbox->addWidget(time_label_);
    timeline_hbox->addWidget(status_tag_);
    ctrl_vbox->addLayout(timeline_hbox);

    // 2.2 第二行: 播放/暂停、快进单帧步进、音量、倍速、截图、全屏
    auto* buttons_hbox = new QHBoxLayout();
    buttons_hbox->setContentsMargins(0, 0, 0, 0);
    buttons_hbox->setSpacing(6);

    btn_play_pause_ = new QPushButton("▶  播放", controls_container_);
    btn_play_pause_->setFixedWidth(78);
    btn_play_pause_->setCursor(Qt::PointingHandCursor);

    btn_stop_ = new QPushButton("⏹  停止", controls_container_);
    btn_stop_->setFixedWidth(68);
    btn_stop_->setCursor(Qt::PointingHandCursor);

    btn_step_back_ = new QPushButton("⏮ 步退", controls_container_);
    btn_step_back_->setToolTip("单帧步退 (-33ms)");
    btn_step_back_->setCursor(Qt::PointingHandCursor);

    btn_step_fwd_ = new QPushButton("步进 ⏭", controls_container_);
    btn_step_fwd_->setToolTip("单帧步进 (+33ms)");
    btn_step_fwd_->setCursor(Qt::PointingHandCursor);

    btn_loop_ = new QPushButton("🔁 循环: 关", controls_container_);
    btn_loop_->setCheckable(true);
    btn_loop_->setCursor(Qt::PointingHandCursor);

    buttons_hbox->addWidget(btn_play_pause_);
    buttons_hbox->addWidget(btn_stop_);
    buttons_hbox->addWidget(btn_step_back_);
    buttons_hbox->addWidget(btn_step_fwd_);
    buttons_hbox->addWidget(btn_loop_);

    buttons_hbox->addStretch();

    // 播放倍速选择
    buttons_hbox->addWidget(new QLabel("倍速:", controls_container_));
    rate_combo_ = new QComboBox(controls_container_);
    rate_combo_->addItems({"0.5x", "0.75x", "1.0x", "1.25x", "1.5x", "2.0x"});
    rate_combo_->setCurrentIndex(2); // 1.0x
    buttons_hbox->addWidget(rate_combo_);

    // 音量调节组件
    btn_mute_ = new QPushButton("🔊", controls_container_);
    btn_mute_->setFixedSize(30, 26);
    btn_mute_->setCursor(Qt::PointingHandCursor);

    volume_slider_ = new QSlider(Qt::Horizontal, controls_container_);
    volume_slider_->setRange(0, 100);
    volume_slider_->setValue(80);
    volume_slider_->setFixedWidth(80);
    volume_slider_->setCursor(Qt::PointingHandCursor);

    buttons_hbox->addWidget(btn_mute_);
    buttons_hbox->addWidget(volume_slider_);

    // 关键帧截屏与全屏按钮
    btn_snapshot_ = new QPushButton("📸 截图", controls_container_);
    btn_snapshot_->setToolTip("捕获当前视频画面帧并保存到本地");
    btn_snapshot_->setCursor(Qt::PointingHandCursor);

    btn_fullscreen_ = new QPushButton("⛶ 全屏", controls_container_);
    btn_fullscreen_->setToolTip("切换视频全屏播放 (按 Esc 或双击退出)");
    btn_fullscreen_->setCursor(Qt::PointingHandCursor);

    buttons_hbox->addWidget(btn_snapshot_);
    buttons_hbox->addWidget(btn_fullscreen_);

    ctrl_vbox->addLayout(buttons_hbox);
    main_layout->addWidget(controls_container_);

    // 3. 信号与槽连接
    connect(btn_play_pause_, &QPushButton::clicked, this, &MediaPlayerWidget::on_play_pause_clicked);
    connect(btn_stop_, &QPushButton::clicked, this, &MediaPlayerWidget::on_stop_clicked);
    connect(btn_step_back_, &QPushButton::clicked, this, [this]() { step_backward(33); });
    connect(btn_step_fwd_, &QPushButton::clicked, this, [this]() { step_forward(33); });
    connect(btn_loop_, &QPushButton::toggled, this, [this](bool checked) {
        set_loop(checked);
        btn_loop_->setText(checked ? "🔁 循环: 开" : "🔁 循环: 关");
    });
    connect(btn_snapshot_, &QPushButton::clicked, this, &MediaPlayerWidget::take_snapshot);
    connect(btn_fullscreen_, &QPushButton::clicked, this, &MediaPlayerWidget::toggle_fullscreen);

    connect(progress_slider_, &QSlider::sliderMoved, this, &MediaPlayerWidget::on_slider_moved);
    connect(progress_slider_, &QSlider::sliderPressed, this, &MediaPlayerWidget::on_slider_pressed);
    connect(progress_slider_, &QSlider::sliderReleased, this, &MediaPlayerWidget::on_slider_released);

    connect(player_, &QMediaPlayer::durationChanged, this, &MediaPlayerWidget::on_duration_changed);
    connect(player_, &QMediaPlayer::positionChanged, this, &MediaPlayerWidget::on_position_changed);
    connect(player_, &QMediaPlayer::mediaStatusChanged, this, &MediaPlayerWidget::on_media_status_changed);
    connect(player_, &QMediaPlayer::playbackStateChanged, this, &MediaPlayerWidget::on_playback_state_changed);

    connect(volume_slider_, &QSlider::valueChanged, this, &MediaPlayerWidget::on_volume_slider_changed);
    connect(btn_mute_, &QPushButton::clicked, this, &MediaPlayerWidget::on_mute_clicked);
    connect(rate_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MediaPlayerWidget::on_rate_changed);
}

void MediaPlayerWidget::load_media(const QString& file_path_or_url) {
    if (file_path_or_url.isEmpty()) return;

    current_path_ = file_path_or_url;
    QUrl media_url;
    if (file_path_or_url.contains("://")) {
        media_url = QUrl(file_path_or_url);
    } else {
        media_url = QUrl::fromLocalFile(file_path_or_url);
    }

    player_->setSource(media_url);
    status_tag_->setText("● 已载入");
    status_tag_->setStyleSheet("color: #58a6ff; font-size: 11px; font-weight: bold;");
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
    progress_slider_->setValue(0);
    update_time_label(0, player_->duration());
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
}

void MediaPlayerWidget::set_volume(int volume_0_to_100) {
    volume_slider_->setValue(volume_0_to_100);
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

    QDir dir("snapshots");
    if (!dir.exists()) dir.mkpath(".");

    QString filename = QString("snapshots/snapshot_%1.png")
                           .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz"));

    QPixmap pix = video_widget_->grab();
    if (pix.save(filename)) {
        status_tag_->setText("📸 截图已保存");
        status_tag_->setStyleSheet("color: #7ee787; font-size: 11px; font-weight: bold;");
        emit snapshot_saved(filename);
    }
}

void MediaPlayerWidget::toggle_fullscreen() {
    if (!video_widget_) return;

    if (is_fullscreen_) {
        video_widget_->setFullScreen(false);
        is_fullscreen_ = false;
        btn_fullscreen_->setText("⛶ 全屏");
    } else {
        video_widget_->setFullScreen(true);
        is_fullscreen_ = true;
        btn_fullscreen_->setText("🗗 退出");
    }
}

void MediaPlayerWidget::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Space) {
        on_play_pause_clicked();
        event->accept();
    } else if (event->key() == Qt::Key_Left) {
        step_backward(1000); // 后退 1 秒
        event->accept();
    } else if (event->key() == Qt::Key_Right) {
        step_forward(1000);  // 前进 1 秒
        event->accept();
    } else if (event->key() == Qt::Key_Escape && is_fullscreen_) {
        toggle_fullscreen();
        event->accept();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void MediaPlayerWidget::on_play_pause_clicked() {
    if (player_->playbackState() == QMediaPlayer::PlayingState) {
        player_->pause();
    } else {
        player_->play();
    }
}

void MediaPlayerWidget::on_stop_clicked() {
    stop();
}

void MediaPlayerWidget::on_slider_moved(int position) {
    update_time_label(position, player_->duration());
}

void MediaPlayerWidget::on_slider_pressed() {
    is_slider_dragging_ = true;
}

void MediaPlayerWidget::on_slider_released() {
    is_slider_dragging_ = false;
    player_->setPosition(progress_slider_->value());
}

void MediaPlayerWidget::on_duration_changed(qint64 duration) {
    progress_slider_->setRange(0, static_cast<int>(duration));
    update_time_label(player_->position(), duration);
}

void MediaPlayerWidget::on_position_changed(qint64 position) {
    if (!is_slider_dragging_) {
        progress_slider_->setValue(static_cast<int>(position));
    }
    update_time_label(position, player_->duration());
    emit position_changed(position);
}

void MediaPlayerWidget::on_media_status_changed(QMediaPlayer::MediaStatus status) {
    if (status == QMediaPlayer::EndOfMedia) {
        if (is_loop_enabled_) {
            player_->setPosition(0);
            player_->play();
        } else {
            status_tag_->setText("● 播放完成");
            status_tag_->setStyleSheet("color: #8b949e; font-size: 11px;");
        }
    } else if (status == QMediaPlayer::InvalidMedia) {
        status_tag_->setText("❌ 媒体解析错误");
        status_tag_->setStyleSheet("color: #f85149; font-size: 11px; font-weight: bold;");
    }
}

void MediaPlayerWidget::on_playback_state_changed(QMediaPlayer::PlaybackState state) {
    if (state == QMediaPlayer::PlayingState) {
        btn_play_pause_->setText("⏸  暂停");
        status_tag_->setText("● 正在播放");
        status_tag_->setStyleSheet("color: #3fb950; font-size: 11px; font-weight: bold;");
    } else if (state == QMediaPlayer::PausedState) {
        btn_play_pause_->setText("▶  继续");
        status_tag_->setText("❚❚ 已暂停");
        status_tag_->setStyleSheet("color: #e3b341; font-size: 11px; font-weight: bold;");
    } else {
        btn_play_pause_->setText("▶  播放");
        status_tag_->setText("⏹ 已停止");
        status_tag_->setStyleSheet("color: #8b949e; font-size: 11px;");
    }
    emit playback_state_changed(state);
}

void MediaPlayerWidget::on_volume_slider_changed(int value) {
    if (!audio_output_) return;
    float vol = static_cast<float>(value) / 100.0f;
    audio_output_->setVolume(vol);

    if (value == 0) {
        btn_mute_->setText("🔇");
    } else if (value < 50) {
        btn_mute_->setText("🔉");
    } else {
        btn_mute_->setText("🔊");
    }
}

void MediaPlayerWidget::on_mute_clicked() {
    if (!audio_output_) return;

    if (audio_output_->isMuted()) {
        audio_output_->setMuted(false);
        volume_slider_->setValue(saved_volume_);
    } else {
        saved_volume_ = volume_slider_->value();
        audio_output_->setMuted(true);
        volume_slider_->setValue(0);
    }
}

void MediaPlayerWidget::on_rate_changed(int index) {
    qreal rate = 1.0;
    switch (index) {
        case 0: rate = 0.5; break;
        case 1: rate = 0.75; break;
        case 2: rate = 1.0; break;
        case 3: rate = 1.25; break;
        case 4: rate = 1.5; break;
        case 5: rate = 2.0; break;
        default: rate = 1.0; break;
    }
    set_playback_rate(rate);
}

void MediaPlayerWidget::update_time_label(qint64 position, qint64 duration) {
    time_label_->setText(QString("%1 / %2").arg(format_time(position), format_time(duration)));
}

QString MediaPlayerWidget::format_time(qint64 ms) {
    int total_secs = static_cast<int>(ms / 1000);
    int minutes = total_secs / 60;
    int seconds = total_secs % 60;
    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}
