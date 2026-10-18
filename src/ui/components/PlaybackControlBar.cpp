#include "PlaybackControlBar.hpp"
#include <QToolTip>
#include <QDateTime>

PlaybackControlBar::PlaybackControlBar(QWidget* parent) : QWidget(parent) {
    setup_ui();
}

void PlaybackControlBar::setup_ui() {
    auto* root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(10, 8, 10, 8);
    root_layout->setSpacing(6);

    setStyleSheet(
        "PlaybackControlBar { background-color: #161b22; border: 1px solid #30363d; border-radius: 6px; } "
        "QPushButton { background-color: #21262d; color: #e6edf3; border: 1px solid #30363d; border-radius: 4px; padding: 4px 8px; font-size: 12px; font-weight: 500; } "
        "QPushButton:hover { background-color: #30363d; border-color: #58a6ff; color: #58a6ff; } "
        "QPushButton:pressed { background-color: #1f6feb; color: #ffffff; } "
        "QPushButton:disabled { color: #484f58; border-color: #21262d; } "
        "QComboBox { background-color: #21262d; color: #e6edf3; border: 1px solid #30363d; border-radius: 4px; padding: 2px 6px; font-size: 11px; } "
        "QComboBox QAbstractItemView { background-color: #161b22; color: #e6edf3; selection-background-color: #1f6feb; border: 1px solid #30363d; } "
        "QSlider::groove:horizontal { height: 4px; background: #21262d; border-radius: 2px; } "
        "QSlider::sub-page:horizontal { background: #58a6ff; border-radius: 2px; } "
        "QSlider::handle:horizontal { width: 12px; margin-top: -4px; margin-bottom: -4px; border-radius: 6px; background: #f0f6fc; } "
        "QSlider::handle:horizontal:hover { background: #58a6ff; }"
    );

    // ==========================================
    // 第一行: 行业标准时间轴排版 [首部时间] ━━━━●━━━━ [尾部时间]
    // ==========================================
    auto* timeline_layout = new QHBoxLayout();
    timeline_layout->setContentsMargins(0, 0, 0, 0);
    timeline_layout->setSpacing(10);

    // 首部时间标签 (当前进度)
    lbl_current_time_ = new QLabel("00:00", this);
    lbl_current_time_->setStyleSheet("color: #58a6ff; font-family: 'Consolas', monospace; font-size: 12px; font-weight: bold; min-width: 50px;");
    timeline_layout->addWidget(lbl_current_time_);

    // 核心进度滑块 (弹性拉伸)
    slider_progress_ = new QSlider(Qt::Horizontal, this);
    slider_progress_->setRange(0, 0);
    slider_progress_->setCursor(Qt::PointingHandCursor);
    slider_progress_->setToolTip("拖动或点击时间轴以快速跳转");
    timeline_layout->addWidget(slider_progress_, 1);

    // 尾部时间标签 (媒体总时长)
    lbl_total_time_ = new QLabel("00:00", this);
    lbl_total_time_->setStyleSheet("color: #8b949e; font-family: 'Consolas', monospace; font-size: 12px; font-weight: bold; min-width: 50px; text-align: right;");
    timeline_layout->addWidget(lbl_total_time_);

    root_layout->addLayout(timeline_layout);

    // ==========================================
    // 第二行: 核心控制操作栏 (播放控制 + 音量 + 倍速 + 功能扩展)
    // ==========================================
    auto* ctrl_layout = new QHBoxLayout();
    ctrl_layout->setContentsMargins(0, 0, 0, 0);
    ctrl_layout->setSpacing(6);

    // 播放/暂停
    btn_play_pause_ = new QPushButton("▶ 播放", this);
    btn_play_pause_->setCursor(Qt::PointingHandCursor);
    btn_play_pause_->setFixedWidth(72);
    ctrl_layout->addWidget(btn_play_pause_);

    // 停止
    btn_stop_ = new QPushButton("■ 停止", this);
    btn_stop_->setCursor(Qt::PointingHandCursor);
    ctrl_layout->addWidget(btn_stop_);

    // 单帧步退与步进 (30FPS ~ 33ms)
    btn_step_back_ = new QPushButton("⏪ 逐帧-", this);
    btn_step_back_->setToolTip("单帧精准步退 (约 33ms)");
    btn_step_back_->setCursor(Qt::PointingHandCursor);
    ctrl_layout->addWidget(btn_step_back_);

    btn_step_fwd_ = new QPushButton("⏩ 逐帧+", this);
    btn_step_fwd_->setToolTip("单帧精准步进 (约 33ms)");
    btn_step_fwd_->setCursor(Qt::PointingHandCursor);
    ctrl_layout->addWidget(btn_step_fwd_);

    // 运行状态小胶囊
    status_badge_ = new QLabel("● 就绪", this);
    status_badge_->setStyleSheet("color: #3fb950; font-size: 11px; font-weight: bold; margin-left: 8px;");
    ctrl_layout->addWidget(status_badge_);

    ctrl_layout->addStretch();

    // 音频控制组: 静音按钮 + 音量滑块 + 百分比标签
    btn_mute_ = new QPushButton("🔊", this);
    btn_mute_->setToolTip("静音 / 恢复声音");
    btn_mute_->setFixedWidth(34);
    btn_mute_->setCursor(Qt::PointingHandCursor);
    ctrl_layout->addWidget(btn_mute_);

    slider_volume_ = new QSlider(Qt::Horizontal, this);
    slider_volume_->setRange(0, 100);
    slider_volume_->setValue(80);
    slider_volume_->setFixedWidth(70);
    slider_volume_->setCursor(Qt::PointingHandCursor);
    slider_volume_->setToolTip("音量调节 (0% ~ 100%)");
    ctrl_layout->addWidget(slider_volume_);

    lbl_volume_val_ = new QLabel("80%", this);
    lbl_volume_val_->setStyleSheet("color: #8b949e; font-size: 11px; min-width: 28px;");
    ctrl_layout->addWidget(lbl_volume_val_);

    ctrl_layout->addSpacing(8);

    // 播放倍速
    combo_rate_ = new QComboBox(this);
    combo_rate_->addItems({"0.5x", "0.75x", "1.0x", "1.25x", "1.5x", "2.0x"});
    combo_rate_->setCurrentIndex(2); // 1.0x
    combo_rate_->setToolTip("播放倍速调整");
    ctrl_layout->addWidget(combo_rate_);

    // 循环播放开关
    btn_loop_ = new QPushButton("🔁 循环: 关", this);
    btn_loop_->setCheckable(true);
    btn_loop_->setToolTip("开启/关闭循环播放");
    btn_loop_->setCursor(Qt::PointingHandCursor);
    ctrl_layout->addWidget(btn_loop_);

    // 抓拍与全屏
    btn_snapshot_ = new QPushButton("📸 抓拍", this);
    btn_snapshot_->setToolTip("截取当前高画质关键帧并保存至 snapshots/ 目录");
    btn_snapshot_->setCursor(Qt::PointingHandCursor);
    ctrl_layout->addWidget(btn_snapshot_);

    btn_fullscreen_ = new QPushButton("⛶ 全屏", this);
    btn_fullscreen_->setToolTip("进入/退出全屏");
    btn_fullscreen_->setCursor(Qt::PointingHandCursor);
    ctrl_layout->addWidget(btn_fullscreen_);

    root_layout->addLayout(ctrl_layout);

    // ==========================================
    // 内部事件连接
    // ==========================================
    connect(btn_play_pause_, &QPushButton::clicked, this, &PlaybackControlBar::on_play_pause_clicked);
    connect(btn_stop_, &QPushButton::clicked, this, &PlaybackControlBar::stop_requested);
    connect(btn_step_back_, &QPushButton::clicked, this, [this]() { emit step_backward_requested(33); });
    connect(btn_step_fwd_, &QPushButton::clicked, this, [this]() { emit step_forward_requested(33); });

    connect(slider_progress_, &QSlider::sliderMoved, this, &PlaybackControlBar::on_slider_moved);
    connect(slider_progress_, &QSlider::sliderPressed, this, &PlaybackControlBar::on_slider_pressed);
    connect(slider_progress_, &QSlider::sliderReleased, this, &PlaybackControlBar::on_slider_released);

    connect(btn_mute_, &QPushButton::clicked, this, &PlaybackControlBar::on_mute_clicked);
    connect(slider_volume_, &QSlider::valueChanged, this, &PlaybackControlBar::on_volume_slider_changed);
    connect(combo_rate_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PlaybackControlBar::on_rate_combo_changed);

    connect(btn_loop_, &QPushButton::toggled, this, [this](bool checked) {
        btn_loop_->setText(checked ? "🔁 循环: 开" : "🔁 循环: 关");
        emit loop_toggled(checked);
    });

    connect(btn_snapshot_, &QPushButton::clicked, this, &PlaybackControlBar::snapshot_requested);
    connect(btn_fullscreen_, &QPushButton::clicked, this, &PlaybackControlBar::fullscreen_requested);
}

void PlaybackControlBar::set_media_mode(bool is_live_stream) {
    is_live_ = is_live_stream;
    if (is_live_) {
        // RTSP 实时流模式: 隐藏/禁用拖拽时间轴与部分不适用操作
        slider_progress_->setEnabled(false);
        slider_progress_->setValue(0);
        lbl_current_time_->setText("🔴 实时直播");
        lbl_current_time_->setStyleSheet("color: #ff4757; font-weight: bold; font-size: 12px;");
        lbl_total_time_->setText("LIVE");
        lbl_total_time_->setStyleSheet("color: #ff4757; font-weight: bold; font-size: 12px;");

        btn_step_back_->setEnabled(false);
        btn_step_fwd_->setEnabled(false);
        combo_rate_->setEnabled(false);
        btn_loop_->setEnabled(false);
        status_badge_->setText("🔴 实时流在线");
        status_badge_->setStyleSheet("color: #ff4757; font-size: 11px; font-weight: bold;");
    } else {
        // 本地视频/切片点播模式: 开启完整交互与首尾时间显示
        slider_progress_->setEnabled(true);
        lbl_current_time_->setStyleSheet("color: #58a6ff; font-family: 'Consolas', monospace; font-size: 12px; font-weight: bold; min-width: 50px;");
        lbl_total_time_->setStyleSheet("color: #8b949e; font-family: 'Consolas', monospace; font-size: 12px; font-weight: bold; min-width: 50px;");

        btn_step_back_->setEnabled(true);
        btn_step_fwd_->setEnabled(true);
        combo_rate_->setEnabled(true);
        btn_loop_->setEnabled(true);
        status_badge_->setText("● 就绪");
        status_badge_->setStyleSheet("color: #3fb950; font-size: 11px; font-weight: bold;");
    }
}

void PlaybackControlBar::set_duration(qint64 duration_ms) {
    duration_ms_ = duration_ms;
    if (!is_live_) {
        slider_progress_->setRange(0, static_cast<int>(duration_ms_));
        lbl_total_time_->setText(format_time(duration_ms_));
    }
}

void PlaybackControlBar::set_position(qint64 position_ms) {
    position_ms_ = position_ms;
    if (!is_live_) {
        if (!is_slider_dragging_) {
            slider_progress_->setValue(static_cast<int>(position_ms_));
        }
        lbl_current_time_->setText(format_time(position_ms_));
    }
}

void PlaybackControlBar::set_playback_state(bool is_playing) {
    is_playing_ = is_playing;
    if (is_playing_) {
        btn_play_pause_->setText("❚❚ 暂停");
        status_badge_->setText("● 播放中");
        status_badge_->setStyleSheet("color: #3fb950; font-size: 11px; font-weight: bold;");
    } else {
        btn_play_pause_->setText("▶ 播放");
        status_badge_->setText("❚❚ 已暂停");
        status_badge_->setStyleSheet("color: #e3b341; font-size: 11px; font-weight: bold;");
    }
}

void PlaybackControlBar::set_volume(int volume_0_100) {
    slider_volume_->setValue(volume_0_100);
    lbl_volume_val_->setText(QString("%1%").arg(volume_0_100));
}

void PlaybackControlBar::set_muted(bool muted) {
    is_muted_ = muted;
    btn_mute_->setText(muted ? "🔇" : "🔊");
}

void PlaybackControlBar::set_status_text(const QString& text, const QString& color_hex) {
    status_badge_->setText(text);
    status_badge_->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold;").arg(color_hex));
}

void PlaybackControlBar::on_play_pause_clicked() {
    if (is_playing_) {
        emit pause_requested();
    } else {
        emit play_requested();
    }
}

void PlaybackControlBar::on_slider_moved(int value) {
    lbl_current_time_->setText(format_time(value));
    QToolTip::showText(QCursor::pos(), format_time(value), slider_progress_);
}

void PlaybackControlBar::on_slider_pressed() {
    is_slider_dragging_ = true;
}

void PlaybackControlBar::on_slider_released() {
    is_slider_dragging_ = false;
    emit seek_requested(slider_progress_->value());
}

void PlaybackControlBar::on_mute_clicked() {
    is_muted_ = !is_muted_;
    btn_mute_->setText(is_muted_ ? "🔇" : "🔊");
    if (is_muted_) {
        saved_volume_ = slider_volume_->value();
        slider_volume_->setValue(0);
        emit mute_toggled(true);
    } else {
        slider_volume_->setValue(saved_volume_ > 0 ? saved_volume_ : 80);
        emit mute_toggled(false);
    }
}

void PlaybackControlBar::on_volume_slider_changed(int value) {
    lbl_volume_val_->setText(QString("%1%").arg(value));
    if (value == 0) {
        is_muted_ = true;
        btn_mute_->setText("🔇");
    } else {
        is_muted_ = false;
        btn_mute_->setText("🔊");
        saved_volume_ = value;
    }
    emit volume_changed(value);
}

void PlaybackControlBar::on_rate_combo_changed(int index) {
    static const double rates[] = {0.5, 0.75, 1.0, 1.25, 1.5, 2.0};
    if (index >= 0 && index < 6) {
        emit rate_changed(rates[index]);
    }
}

QString PlaybackControlBar::format_time(qint64 ms) {
    qint64 total_sec = ms / 1000;
    qint64 h = total_sec / 3600;
    qint64 m = (total_sec % 3600) / 60;
    qint64 s = total_sec % 60;

    if (h > 0) {
        return QString("%1:%2:%3")
            .arg(h, 2, 10, QChar('0'))
            .arg(m, 2, 10, QChar('0'))
            .arg(s, 2, 10, QChar('0'));
    }
    return QString("%1:%2")
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'));
}
