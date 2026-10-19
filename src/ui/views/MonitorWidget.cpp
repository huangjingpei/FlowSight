#include "MonitorWidget.hpp"
#include "ui/VideoCanvas.hpp"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QDateTime>
#include <QPushButton>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QToolTip>
#include <QScrollArea>

MonitorWidget::MonitorWidget(QWidget* parent) : QWidget(parent) {
    setup_ui();
}

void MonitorWidget::setup_ui() {
    auto* main_layout = new QHBoxLayout(this);
    main_layout->setContentsMargins(10, 10, 10, 10);
    main_layout->setSpacing(10);

    // =========================================================================
    // 1. 左侧视频监控区 (标准 NVR 监视窗口: 顶部通道标头 + 视频画面 + 时间轴控制条)
    // =========================================================================
    video_container_ = new QWidget(this);
    auto* video_vbox = new QVBoxLayout(video_container_);
    video_vbox->setContentsMargins(0, 0, 0, 0);
    video_vbox->setSpacing(6);

    // 1.1 视频通道 OSD 状态顶栏
    auto* video_top_bar = new QWidget(video_container_);
    video_top_bar->setFixedHeight(34);
    video_top_bar->setStyleSheet("background-color: #16181d; border: 1px solid #282c34; border-radius: 3px;");
    auto* top_hbox = new QHBoxLayout(video_top_bar);
    top_hbox->setContentsMargins(10, 0, 10, 0);
    top_hbox->setSpacing(8);

    auto* rec_dot = new QLabel("● [CAM-01] 实时在线分析", video_top_bar);
    rec_dot->setStyleSheet("color: #3fb950; font-weight: bold; font-size: 11px;");

    res_tag_ = new QLabel("模式: 准备就绪  |  音频: 已启用", video_top_bar);
    res_tag_->setStyleSheet("color: #8c96a3; font-size: 11px;");

    auto* btn_load_video = new QPushButton("📁 载入视频...", video_top_bar);
    btn_load_video->setCursor(Qt::PointingHandCursor);
    btn_load_video->setStyleSheet(
        "QPushButton { background-color: #1f2329; color: #58a6ff; border: 1px solid #2d323b; border-radius: 3px; padding: 3px 10px; font-weight: 600; font-size: 11px; } "
        "QPushButton:hover { background-color: #282c34; border-color: #58a6ff; color: #ffffff; }"
    );
    connect(btn_load_video, &QPushButton::clicked, this, [this]() {
        QString file = QFileDialog::getOpenFileName(this, "选择客流分析 MP4 视频", "", "Video Files (*.mp4 *.avi *.mkv *.mov)");
        if (!file.isEmpty()) {
            QFileInfo fi(file);
            if (res_tag_) {
                res_tag_->setText(QString("视频: %1  |  声画同步分析中").arg(fi.fileName()));
            }
            emit request_change_video(file);
        }
    });

    auto* btn_snapshot = new QPushButton("📷 抓拍", video_top_bar);
    btn_snapshot->setCursor(Qt::PointingHandCursor);
    btn_snapshot->setStyleSheet(
        "QPushButton { background-color: #1f2329; color: #c9d1d9; border: 1px solid #2d323b; border-radius: 3px; padding: 3px 8px; font-size: 11px; } "
        "QPushButton:hover { background-color: #282c34; color: #ffffff; }"
    );
    connect(btn_snapshot, &QPushButton::clicked, this, &MonitorWidget::take_snapshot);

    auto* btn_fullscreen = new QPushButton("⛶ 全屏", video_top_bar);
    btn_fullscreen->setCursor(Qt::PointingHandCursor);
    btn_fullscreen->setStyleSheet(
        "QPushButton { background-color: #1f2329; color: #c9d1d9; border: 1px solid #2d323b; border-radius: 3px; padding: 3px 8px; font-size: 11px; } "
        "QPushButton:hover { background-color: #282c34; color: #ffffff; }"
    );
    connect(btn_fullscreen, &QPushButton::clicked, this, &MonitorWidget::toggle_fullscreen);

    top_hbox->addWidget(rec_dot);
    top_hbox->addSpacing(10);
    top_hbox->addWidget(res_tag_);
    top_hbox->addStretch();
    top_hbox->addWidget(btn_load_video);
    top_hbox->addWidget(btn_snapshot);
    top_hbox->addWidget(btn_fullscreen);
    video_vbox->addWidget(video_top_bar);

    // 1.2 专业 AI 监控视频画布
    canvas_ = new VideoCanvas(video_container_);
    canvas_->set_read_only(true);
    canvas_->setStyleSheet("border: 1px solid #282c34; border-radius: 3px; background-color: #0d0f12;");
    video_vbox->addWidget(canvas_, 1);

    // 1.3 工业标准首尾时间轴播放控制条
    control_bar_ = new PlaybackControlBar(video_container_);
    connect(control_bar_, &PlaybackControlBar::snapshot_requested, this, &MonitorWidget::take_snapshot);
    connect(control_bar_, &PlaybackControlBar::fullscreen_requested, this, &MonitorWidget::toggle_fullscreen);
    video_vbox->addWidget(control_bar_);

    main_layout->addWidget(video_container_, 1);

    // =========================================================================
    // 2. 右侧 NVR 核心监控操作台 (通道布防 + 客流仪表矩阵 + 承载风控 + 实时流水)
    // =========================================================================
    auto* right_scroll = new QScrollArea(this);
    right_scroll->setFixedWidth(310);
    right_scroll->setWidgetResizable(true);
    right_scroll->setFrameShape(QFrame::NoFrame);
    right_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    right_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto* right_panel = new QWidget(right_scroll);
    auto* right_vbox = new QVBoxLayout(right_panel);
    right_vbox->setContentsMargins(0, 0, 4, 0);
    right_vbox->setSpacing(8);

    // -------------------------------------------------------------------------
    // 模块 1: 通道布防信息卡片 (NVR Channel Information)
    // -------------------------------------------------------------------------
    auto* channel_card = new QWidget(right_panel);
    channel_card->setObjectName("nvr_channel_card");
    channel_card->setStyleSheet(
        "QWidget#nvr_channel_card { background-color: #1a1d22; border: 1px solid #282c34; border-radius: 3px; } "
        "QWidget#nvr_channel_card QLabel { background: transparent; border: none; padding: 0px; margin: 0px; }"
    );
    auto* ch_vbox = new QVBoxLayout(channel_card);
    ch_vbox->setContentsMargins(10, 8, 10, 8);
    ch_vbox->setSpacing(4);

    auto* ch_hdr = new QHBoxLayout();
    ch_hdr->setContentsMargins(0, 0, 0, 0);
    auto* ch_dot = new QLabel("●", channel_card);
    ch_dot->setStyleSheet("color: #3fb950; font-size: 13px; font-weight: bold;");
    auto* ch_title = new QLabel("通道布防状态  CAM-01 [A门通道]", channel_card);
    ch_title->setStyleSheet("color: #e6edf3; font-size: 12px; font-weight: bold;");
    ch_hdr->addWidget(ch_dot);
    ch_hdr->addWidget(ch_title);
    ch_hdr->addStretch();

    channel_status_label_ = new QLabel("布防状态: 在线分析中  |  双模态客流检测", channel_card);
    channel_status_label_->setStyleSheet("color: #8c96a3; font-size: 11px;");

    channel_info_label_ = new QLabel("AI 架构: YOLOv8n + Beit-INT8 + InsightFace", channel_card);
    channel_info_label_->setStyleSheet("color: #57606a; font-size: 10px; font-family: 'Consolas', monospace;");

    ch_vbox->addLayout(ch_hdr);
    ch_vbox->addWidget(channel_status_label_);
    ch_vbox->addWidget(channel_info_label_);
    right_vbox->addWidget(channel_card);

    // -------------------------------------------------------------------------
    // 模块 2: 4 大核心客流数显仪表矩阵 (一体化排版，彻底杜绝 QSS 碎块分裂)
    // -------------------------------------------------------------------------
    auto* c1 = create_nvr_kpi_card("kpi_occupancy", "当前区域人数 (滞留)", "0", "人", "#00b4d8", "● 瞬时在场负荷监控中", occupancy_label_, occupancy_sub_label_);
    right_vbox->addWidget(c1);

    QLabel* dummy_sub2 = nullptr;
    auto* c2 = create_nvr_kpi_card("kpi_entry", "今日累计进入 (In)", "0", "人次", "#2ecb71", "↑ 进门通行累计", entry_label_, dummy_sub2);
    right_vbox->addWidget(c2);

    QLabel* dummy_sub3 = nullptr;
    auto* c3 = create_nvr_kpi_card("kpi_exit", "今日累计离开 (Out)", "0", "人次", "#f85149", "↓ 出门通行累计", exit_label_, dummy_sub3);
    right_vbox->addWidget(c3);

    QLabel* dummy_sub4 = nullptr;
    auto* c4 = create_nvr_kpi_card("kpi_total", "全天通行总负荷 (Total)", "0", "人次", "#ffa502", "∑ 进出双向通行汇总", total_label_, dummy_sub4);
    right_vbox->addWidget(c4);

    // -------------------------------------------------------------------------
    // 模块 3: 区域人员安全承载风控条 (Crowd Capacity Gauge)
    // -------------------------------------------------------------------------
    auto* surge_card = new QWidget(right_panel);
    surge_card->setObjectName("nvr_surge_card");
    surge_card->setStyleSheet(
        "QWidget#nvr_surge_card { background-color: #1a1d22; border: 1px solid #282c34; border-radius: 3px; } "
        "QWidget#nvr_surge_card QLabel { background: transparent; border: none; padding: 0px; margin: 0px; }"
    );
    auto* s_vbox = new QVBoxLayout(surge_card);
    s_vbox->setContentsMargins(10, 8, 10, 8);
    s_vbox->setSpacing(6);

    auto* s_header = new QHBoxLayout();
    s_header->setContentsMargins(0, 0, 0, 0);
    auto* s_title = new QLabel("区域安全承载负荷", surge_card);
    s_title->setStyleSheet("color: #8c96a3; font-size: 11px; font-weight: 600;");
    surge_text_label_ = new QLabel("0 / 30 人 (0%)", surge_card);
    surge_text_label_->setStyleSheet("color: #58a6ff; font-size: 11px; font-weight: bold;");
    s_header->addWidget(s_title);
    s_header->addStretch();
    s_header->addWidget(surge_text_label_);

    surge_progress_ = new QProgressBar(surge_card);
    surge_progress_->setRange(0, 100);
    surge_progress_->setValue(0);
    surge_progress_->setTextVisible(false);
    surge_progress_->setFixedHeight(5);
    surge_progress_->setStyleSheet(
        "QProgressBar { background-color: #121417; border: none; border-radius: 2px; } "
        "QProgressBar::chunk { background-color: #2ecb71; border-radius: 2px; }"
    );

    s_vbox->addLayout(s_header);
    s_vbox->addWidget(surge_progress_);
    right_vbox->addWidget(surge_card);

    // -------------------------------------------------------------------------
    // 模块 4: 实时通行抓拍记录审计流 (Passage Event Stream - 置顶滚动)
    // -------------------------------------------------------------------------
    auto* event_card = new QWidget(right_panel);
    event_card->setObjectName("nvr_event_card");
    event_card->setStyleSheet(
        "QWidget#nvr_event_card { background-color: #1a1d22; border: 1px solid #282c34; border-radius: 3px; } "
        "QWidget#nvr_event_card QLabel { background: transparent; border: none; padding: 0px; margin: 0px; }"
    );
    auto* ev_vbox = new QVBoxLayout(event_card);
    ev_vbox->setContentsMargins(10, 8, 10, 8);
    ev_vbox->setSpacing(6);

    auto* ev_header = new QHBoxLayout();
    ev_header->setContentsMargins(0, 0, 0, 0);
    auto* ev_title = new QLabel("📋 实时通行流水记录", event_card);
    ev_title->setStyleSheet("color: #8c96a3; font-size: 11px; font-weight: 600;");
    auto* ev_badge = new QLabel("最新 50 条", event_card);
    ev_badge->setStyleSheet("color: #57606a; font-size: 10px;");
    ev_header->addWidget(ev_title);
    ev_header->addStretch();
    ev_header->addWidget(ev_badge);

    ticker_list_ = new QListWidget(event_card);
    ticker_list_->setObjectName("nvr_event_list");
    ticker_list_->setFrameShape(QFrame::NoFrame);
    ticker_list_->setSelectionMode(QAbstractItemView::NoSelection);
    ticker_list_->setMinimumHeight(180);
    ticker_list_->setStyleSheet(
        "QListWidget#nvr_event_list { background: transparent; border: none; outline: none; } "
        "QListWidget#nvr_event_list::item { "
        "  background-color: #16181d; "
        "  border: 1px solid #23272e; "
        "  border-radius: 2px; "
        "  padding: 4px 6px; "
        "  margin-bottom: 3px; "
        "  font-family: 'Segoe UI', 'Consolas', monospace; "
        "  font-size: 11px; "
        "  color: #c9d1d9; "
        "} "
        "QListWidget#nvr_event_list::item:hover { background-color: #20242b; border-color: #383f4a; }"
    );

    ev_vbox->addLayout(ev_header);
    ev_vbox->addWidget(ticker_list_, 1);
    right_vbox->addWidget(event_card, 1);

    right_scroll->setWidget(right_panel);
    main_layout->addWidget(right_scroll, 0);
}

QWidget* MonitorWidget::create_nvr_kpi_card(const QString& card_id, const QString& title, const QString& init_val,
                                            const QString& unit, const QString& accent_color, const QString& sub_info,
                                            QLabel*& out_val_label, QLabel*& out_sub_label) {
    auto* card = new QWidget();
    card->setObjectName(card_id);

    // 精准限定样式仅作用于该 Card，内部所有 QLabel 彻底清除边框与背景
    card->setStyleSheet(QString(
        "QWidget#%1 { "
        "  background-color: #1a1d22; "
        "  border: 1px solid #282c34; "
        "  border-left: 3.5px solid %2; "
        "  border-radius: 3px; "
        "} "
        "QWidget#%1 QLabel { "
        "  background: transparent; "
        "  border: none; "
        "  padding: 0px; "
        "  margin: 0px; "
        "}"
    ).arg(card_id, accent_color));

    auto* vbox = new QVBoxLayout(card);
    vbox->setContentsMargins(10, 8, 10, 8);
    vbox->setSpacing(2);

    // 1. 顶部指标小标题
    auto* top_hbox = new QHBoxLayout();
    top_hbox->setContentsMargins(0, 0, 0, 0);
    top_hbox->setSpacing(6);

    auto* t_lbl = new QLabel(title, card);
    t_lbl->setStyleSheet("color: #8c96a3; font-size: 11px; font-weight: 600; letter-spacing: 0.3px;");
    top_hbox->addWidget(t_lbl);
    top_hbox->addStretch();

    // 2. 核心大数值与单位（一体化并列排版，绝无分裂独立框）
    auto* val_hbox = new QHBoxLayout();
    val_hbox->setContentsMargins(0, 0, 0, 0);
    val_hbox->setSpacing(4);

    out_val_label = new QLabel(init_val, card);
    out_val_label->setStyleSheet("color: #f0f6fc; font-size: 26px; font-weight: bold; font-family: 'Segoe UI', 'Consolas', 'Arial', sans-serif;");

    auto* u_lbl = new QLabel(unit, card);
    u_lbl->setStyleSheet("color: #6e7681; font-size: 11px; font-weight: 500; padding-bottom: 3px;");

    val_hbox->addWidget(out_val_label);
    val_hbox->addWidget(u_lbl, 0, Qt::AlignBottom);
    val_hbox->addStretch();

    // 3. 底部状态辅助说明
    out_sub_label = new QLabel(sub_info, card);
    out_sub_label->setStyleSheet("color: #57606a; font-size: 10px;");

    vbox->addLayout(top_hbox);
    vbox->addLayout(val_hbox);
    vbox->addWidget(out_sub_label);

    return card;
}

void MonitorWidget::update_frame(const QImage& frame, const std::vector<TrackObject>& tracks) {
    if (canvas_) {
        canvas_->update_frame(frame, tracks);
    }
}

void MonitorWidget::update_stats(const StatisticsSnapshot& stats) {
    current_occupancy_ = stats.current_occupancy;

    if (occupancy_label_) {
        occupancy_label_->setText(QString::number(stats.current_occupancy));
    }
    if (entry_label_) entry_label_->setText(QString::number(stats.today_entry));
    if (exit_label_) exit_label_->setText(QString::number(stats.today_exit));
    if (total_label_) total_label_->setText(QString::number(stats.today_total));

    // 动态刷新荷载风控进度条
    if (surge_progress_ && surge_text_label_) {
        int pct = (surge_threshold_ > 0) ? std::min(100, (stats.current_occupancy * 100) / surge_threshold_) : 0;
        surge_progress_->setValue(pct);
        surge_text_label_->setText(QString("%1 / %2 人 (%3%)").arg(stats.current_occupancy).arg(surge_threshold_).arg(pct));
    }
}

void MonitorWidget::set_crowd_surge_alert(bool is_surged, int current_occupancy, int threshold) {
    current_occupancy_ = current_occupancy;
    surge_threshold_ = threshold > 0 ? threshold : 30;

    if (canvas_) {
        canvas_->set_crowd_surge_alert(is_surged, current_occupancy, threshold);
    }

    if (occupancy_label_) {
        occupancy_label_->setText(QString::number(current_occupancy));
        if (is_surged) {
            occupancy_label_->setStyleSheet("color: #f85149; font-size: 26px; font-weight: bold; font-family: 'Segoe UI', 'Consolas', sans-serif;");
            if (occupancy_sub_label_) {
                occupancy_sub_label_->setText("⚠️ 区域人员超载告警中！");
                occupancy_sub_label_->setStyleSheet("color: #f85149; font-size: 10px; font-weight: bold;");
            }
        } else {
            occupancy_label_->setStyleSheet("color: #f0f6fc; font-size: 26px; font-weight: bold; font-family: 'Segoe UI', 'Consolas', sans-serif;");
            if (occupancy_sub_label_) {
                occupancy_sub_label_->setText("● 瞬时在场负荷监控中");
                occupancy_sub_label_->setStyleSheet("color: #57606a; font-size: 10px;");
            }
        }
    }

    if (surge_progress_ && surge_text_label_) {
        int pct = (surge_threshold_ > 0) ? std::min(100, (current_occupancy * 100) / surge_threshold_) : 0;
        surge_progress_->setValue(pct);
        surge_text_label_->setText(QString("%1 / %2 人 (%3%)").arg(current_occupancy).arg(surge_threshold_).arg(pct));

        if (is_surged || pct >= 100) {
            surge_progress_->setStyleSheet(
                "QProgressBar { background-color: #121417; border: none; border-radius: 2px; } "
                "QProgressBar::chunk { background-color: #f85149; border-radius: 2px; }"
            );
            surge_text_label_->setStyleSheet("color: #f85149; font-size: 11px; font-weight: bold;");
        } else if (pct >= 70) {
            surge_progress_->setStyleSheet(
                "QProgressBar { background-color: #121417; border: none; border-radius: 2px; } "
                "QProgressBar::chunk { background-color: #ffa502; border-radius: 2px; }"
            );
            surge_text_label_->setStyleSheet("color: #ffa502; font-size: 11px; font-weight: bold;");
        } else {
            surge_progress_->setStyleSheet(
                "QProgressBar { background-color: #121417; border: none; border-radius: 2px; } "
                "QProgressBar::chunk { background-color: #2ecb71; border-radius: 2px; }"
            );
            surge_text_label_->setStyleSheet("color: #58a6ff; font-size: 11px; font-weight: bold;");
        }
    }

    if (is_surged && ticker_list_) {
        QString warn_text = QString("%1  ⚠️ [超载告警] 场内: %2人 (上限: %3人)")
            .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
            .arg(current_occupancy).arg(threshold);
        auto* item = new QListWidgetItem(warn_text);
        item->setForeground(QColor("#f85149"));
        item->setBackground(QColor(50, 15, 20));
        ticker_list_->insertItem(0, item);
    }
}

void MonitorWidget::add_passage_event(const PassageEvent& ev) {
    if (!ticker_list_) return;

    QString time_str = QDateTime::fromMSecsSinceEpoch(ev.timestamp_ms).toString("hh:mm:ss");
    bool is_entry = (ev.direction == PassageDirection::ENTRY);
    QString dir_tag = is_entry ? "🟢 进" : "🔴 出";
    QString zone_str = QString("[%1]").arg(QString::fromStdString(ev.roi_name.empty() ? "A门通道" : ev.roi_name));

    QString attr_str;
    if (!ev.gender.empty() && ev.gender != "未知") {
        attr_str = QString("%1").arg(QString::fromStdString(ev.gender));
        if (!ev.age_group.empty() && ev.age_group != "未知") {
            attr_str += QString("/%1").arg(QString::fromStdString(ev.age_group));
        }
    } else {
        attr_str = "分析中";
    }

    QString text = QString("%1  %2 %3  ID:%4  (%5)")
        .arg(time_str)
        .arg(dir_tag)
        .arg(zone_str)
        .arg(ev.track_id)
        .arg(attr_str);

    auto* item = new QListWidgetItem(text);
    if (is_entry) {
        item->setForeground(QColor("#3fb950")); // 安防绿
    } else {
        item->setForeground(QColor("#f85149")); // 安防红
    }
    ticker_list_->insertItem(0, item);

    while (ticker_list_->count() > 50) {
        delete ticker_list_->takeItem(ticker_list_->count() - 1);
    }
}

void MonitorWidget::update_active_rois(const std::vector<RoiConfig>& rois) {
    if (canvas_) {
        canvas_->set_rois(rois);
    }
}

void MonitorWidget::update_active_roi(const RoiConfig& roi) {
    update_active_rois({roi});
}

void MonitorWidget::set_media_info(bool is_live, qint64 duration_ms, double fps) {
    if (control_bar_) {
        control_bar_->set_media_mode(is_live);
        if (!is_live) {
            control_bar_->set_duration(duration_ms);
        }
    }
    if (res_tag_) {
        if (is_live) {
            res_tag_->setText(QString("模式: RTSP 实时监控流  |  帧率: %1 FPS").arg(QString::number(fps, 'f', 1)));
        } else {
            res_tag_->setText(QString("模式: 本地视频点播  |  时长: %1s  |  声画同步: 在线")
                .arg(QString::number(duration_ms / 1000.0, 'f', 1)));
        }
    }
    if (channel_info_label_) {
        channel_info_label_->setText(QString("码流规格: %1 FPS  |  YOLOv8 + Beit-INT8 + InsightFace")
            .arg(QString::number(fps, 'f', 1)));
    }
}

void MonitorWidget::set_playback_position(qint64 pos_ms) {
    if (control_bar_) {
        control_bar_->set_position(pos_ms);
    }
}

void MonitorWidget::set_playback_state(bool is_playing) {
    if (control_bar_) {
        control_bar_->set_playback_state(is_playing);
    }
}

void MonitorWidget::take_snapshot() {
    if (!canvas_) return;

    QDir().mkpath("snapshots");
    QString filename = QString("snapshots/monitor_%1.png")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz"));

    QPixmap pixmap = canvas_->grab();
    if (pixmap.isNull()) {
        QMessageBox::warning(this, "抓拍失败", "未能从视频画布抓取到图像");
        return;
    }

    if (pixmap.save(filename, "PNG")) {
        QToolTip::showText(mapToGlobal(QPoint(width() / 2, height() / 2)),
                           QString("✅ 监控画面抓拍已保存:\n%1").arg(filename), this);
    } else {
        QMessageBox::warning(this, "保存失败", QString("无法保存抓拍图片至:\n%1").arg(filename));
    }
}

void MonitorWidget::toggle_fullscreen() {
    if (!canvas_) return;

    if (!is_fullscreen_) {
        canvas_->setParent(nullptr);
        canvas_->setWindowFlags(Qt::Window);
        canvas_->showFullScreen();
        is_fullscreen_ = true;
    } else {
        canvas_->setWindowFlags(Qt::Widget);
        static_cast<QVBoxLayout*>(video_container_->layout())->insertWidget(1, canvas_, 1);
        canvas_->showNormal();
        is_fullscreen_ = false;
    }
}
