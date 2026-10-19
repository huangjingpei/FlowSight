#include "EvidenceWidget.hpp"
#include "ui/components/MediaPlayerWidget.hpp"
#include "storage/DatabaseManager.hpp"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QDateTime>
#include <QFileInfo>
#include <QUrl>
#include <QMessageBox>
#include <QFileDialog>
#include <QScrollArea>
#include <QSplitter>

EvidenceWidget::EvidenceWidget(QWidget* parent) : QWidget(parent) {
    setup_ui();
    refresh_events();
}

void EvidenceWidget::setup_ui() {
    auto* main_layout = new QHBoxLayout(this);
    main_layout->setContentsMargins(12, 12, 12, 12);
    main_layout->setSpacing(0);

    // 1. 左侧表格与多维过滤面板
    auto* left_panel = new QWidget(this);
    auto* left_vbox = new QVBoxLayout(left_panel);
    left_vbox->setContentsMargins(0, 0, 0, 0);
    left_vbox->setSpacing(10);

    // 过滤工具栏 1: 方向、时间跨度、搜索框、重置与计数
    auto* filter_bar_1 = new QHBoxLayout();
    filter_bar_1->setSpacing(8);

    btn_refresh_ = new QPushButton("🔄  刷新流水", this);
    btn_refresh_->setFixedHeight(32);
    filter_bar_1->addWidget(btn_refresh_);

    combo_direction_ = new QComboBox(this);
    combo_direction_->setFixedHeight(32);
    combo_direction_->addItems({"全部通行方向 (All)", "🟢 仅看进入 [IN]", "🔴 仅看离开 [OUT]"});
    combo_direction_->setStyleSheet(
        "QComboBox { background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 2px 10px; font-size: 12px; } "
        "QComboBox::drop-down { border: none; } "
        "QComboBox QAbstractItemView { background-color: #161b22; color: #c9d1d9; selection-background-color: #1f6feb; border: 1px solid #30363d; }"
    );
    filter_bar_1->addWidget(combo_direction_);

    combo_timerange_ = new QComboBox(this);
    combo_timerange_->setFixedHeight(32);
    combo_timerange_->addItems({"全部时间跨度", "📅 今日流水 (Today)", "📅 最近 3 天", "📅 最近 7 天"});
    combo_timerange_->setStyleSheet(
        "QComboBox { background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 2px 10px; font-size: 12px; } "
        "QComboBox::drop-down { border: none; } "
        "QComboBox QAbstractItemView { background-color: #161b22; color: #c9d1d9; selection-background-color: #1f6feb; border: 1px solid #30363d; }"
    );
    filter_bar_1->addWidget(combo_timerange_);

    search_input_ = new QLineEdit(this);
    search_input_->setPlaceholderText("🔍 搜索 Track ID / 属性 / 切片...");
    search_input_->setFixedHeight(32);
    search_input_->setStyleSheet(
        "QLineEdit { background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 2px 8px; font-size: 12px; } "
        "QLineEdit:focus { border: 1px solid #58a6ff; }"
    );
    filter_bar_1->addWidget(search_input_, 1);

    btn_reset_filter_ = new QPushButton("重置筛选", this);
    btn_reset_filter_->setFixedHeight(32);
    btn_reset_filter_->setStyleSheet(
        "QPushButton { background-color: #21262d; color: #8b949e; border: 1px solid #30363d; border-radius: 4px; padding: 2px 10px; font-size: 12px; } "
        "QPushButton:hover { background-color: #30363d; color: #c9d1d9; }"
    );
    filter_bar_1->addWidget(btn_reset_filter_);

    count_badge_ = new QLabel("共 0 条证据流水", this);
    count_badge_->setStyleSheet("color: #58a6ff; font-weight: bold; background: #21262d; border: 1px solid #30363d; border-radius: 4px; padding: 4px 10px; font-size: 12px;");
    filter_bar_1->addWidget(count_badge_);

    left_vbox->addLayout(filter_bar_1);

    // 过滤工具栏 2: 打开外部媒体与操作提示
    auto* filter_bar_2 = new QHBoxLayout();
    filter_bar_2->setSpacing(10);

    btn_open_local_ = new QPushButton("📁  打开外部媒体审查...", this);
    btn_open_local_->setFixedHeight(30);
    btn_open_local_->setStyleSheet(
        "QPushButton { background-color: #21262d; color: #58a6ff; border: 1px solid #30363d; border-radius: 4px; padding: 2px 12px; font-weight: 600; font-size: 12px; } "
        "QPushButton:hover { background-color: #30363d; border-color: #58a6ff; color: #ffffff; }"
    );
    connect(btn_open_local_, &QPushButton::clicked, this, [this]() {
        QString file = QFileDialog::getOpenFileName(this, "选择本地音视频进行专业审查回放", "", "Media Files (*.mp4 *.avi *.mkv *.mov *.mp3 *.wav)");
        if (!file.isEmpty()) {
            play_clip(file);
        }
    });

    tip_label_ = new QLabel("💡 提示: 单击查看元数据详情，双击或点击右下角按钮即可声画同步回放", this);
    tip_label_->setStyleSheet("color: #8b949e; font-size: 12px;");

    filter_bar_2->addWidget(btn_open_local_);
    filter_bar_2->addWidget(tip_label_);
    filter_bar_2->addStretch();
    left_vbox->addLayout(filter_bar_2);

    // 证据表格定义
    table_ = new QTableWidget(this);
    table_->setColumnCount(8);
    table_->setHorizontalHeaderLabels({"发生时间", "标定防区", "通行方向", "目标 ID", "性别", "年龄段", "置信度", "短视频证据切片"});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table_->horizontalHeader()->setMinimumSectionSize(55);
    table_->setColumnWidth(0, 160); // "2026-10-18 20:00:23"
    table_->setColumnWidth(1, 135); // "主要出入口防区 #1"
    table_->setColumnWidth(2, 120); // "🟢 进入 [IN]" / "🔴 离开 [OUT]" (避免截断)
    table_->setColumnWidth(3, 80);  // "#1024"
    table_->setColumnWidth(4, 65);  // "男" / "女"
    table_->setColumnWidth(5, 125); // "青年(18-35)" / "中年(36-55)" (避免截断)
    table_->setColumnWidth(6, 80);  // "94.2%"
    table_->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Stretch); // 剩余空间由视频路径填满
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->verticalHeader()->setVisible(false);
    table_->setStyleSheet(
        "QTableWidget { background-color: #0d1117; border: 1px solid #30363d; gridline-color: #21262d; color: #c9d1d9; font-size: 12px; } "
        "QTableWidget::item:selected { background-color: #1f6feb; color: #ffffff; } "
        "QHeaderView::section { background-color: #161b22; color: #8b949e; font-weight: bold; border: 1px solid #21262d; padding: 4px; }"
    );
    left_vbox->addWidget(table_);
    left_panel->setMinimumWidth(580);

    // 2. 右侧专业音视频回放与元数据检查器 (自适应伸缩，支持随全屏最大化宽屏展开)
    auto* right_scroll = new QScrollArea(this);
    right_scroll->setMinimumWidth(380);
    right_scroll->setWidgetResizable(true);
    right_scroll->setFrameShape(QFrame::NoFrame);
    right_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    right_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    player_container_ = new QWidget(right_scroll);
    auto* player_vbox = new QVBoxLayout(player_container_);
    player_vbox->setContentsMargins(0, 0, 6, 0);
    player_vbox->setSpacing(10);

    // 播放器卡片
    auto* p_card = new QWidget(this);
    p_card->setStyleSheet("background-color: #161b22; border: 1px solid #30363d; border-radius: 8px; padding: 10px;");
    auto* card_layout = new QVBoxLayout(p_card);
    card_layout->setContentsMargins(4, 4, 4, 4);
    card_layout->setSpacing(8);

    auto* p_title = new QLabel("🎬  通行事件证据视频与声音溯源回放", p_card);
    p_title->setStyleSheet("font-weight: bold; color: #58a6ff; font-size: 14px;");
    card_layout->addWidget(p_title);

    player_ = new MediaPlayerWidget(p_card);
    card_layout->addWidget(player_, 1);
    player_vbox->addWidget(p_card);

    // 元数据检查器卡片
    auto* meta_card = new QWidget(this);
    meta_card->setStyleSheet("background-color: #161b22; border: 1px solid #30363d; border-radius: 8px; padding: 12px;");
    auto* meta_vbox = new QVBoxLayout(meta_card);
    meta_vbox->setContentsMargins(8, 8, 8, 8);
    meta_vbox->setSpacing(8);

    auto* meta_header = new QHBoxLayout();
    auto* meta_title = new QLabel("📋  证据切片元数据审查", meta_card);
    meta_title->setStyleSheet("font-weight: bold; color: #c9d1d9; font-size: 13px;");
    meta_header->addWidget(meta_title);
    meta_header->addStretch();
    meta_vbox->addLayout(meta_header);

    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(8);
    grid->setColumnStretch(0, 0);
    grid->setColumnStretch(1, 1);

    auto add_row = [&](int r, const QString& label_str, QLabel*& val_label) {
        auto* title = new QLabel(label_str, meta_card);
        title->setStyleSheet("color: #8b949e; font-size: 12px; font-weight: 500;");
        title->setFixedWidth(68);
        val_label = new QLabel("--", meta_card);
        val_label->setStyleSheet("color: #f0f6fc; font-size: 12px; font-weight: 500;");
        val_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        grid->addWidget(title, r, 0, Qt::AlignTop | Qt::AlignLeft);
        grid->addWidget(val_label, r, 1, Qt::AlignVCenter | Qt::AlignLeft);
    };

    add_row(0, "发生时间:", inspector_time_);
    add_row(1, "通行判定:", inspector_dir_);
    add_row(2, "跟踪目标:", inspector_track_);
    add_row(3, "人体属性:", inspector_attr_);
    add_row(4, "切片文件:", inspector_file_);
    inspector_file_->setWordWrap(true);
    add_row(5, "切片状态:", inspector_status_);

    meta_vbox->addLayout(grid);

    btn_inspect_play_ = new QPushButton("▶  立即起播此证据切片", meta_card);
    btn_inspect_play_->setObjectName("btn_primary");
    btn_inspect_play_->setFixedHeight(34);
    btn_inspect_play_->setEnabled(false);
    connect(btn_inspect_play_, &QPushButton::clicked, this, [this]() {
        if (!selected_clip_path_.isEmpty()) {
            play_clip(selected_clip_path_);
        }
    });
    meta_vbox->addWidget(btn_inspect_play_);

    player_vbox->addWidget(meta_card);
    player_vbox->addStretch();

    right_scroll->setWidget(player_container_);

    // 使用 QSplitter 实现专业 NVR 客户端左右弹性分割，最大化全屏时比例自适应拉伸
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(5);
    splitter->setStyleSheet(
        "QSplitter::handle { background-color: #21262d; } "
        "QSplitter::handle:hover { background-color: #58a6ff; }"
    );
    splitter->addWidget(left_panel);
    splitter->addWidget(right_scroll);
    splitter->setCollapsible(0, false);
    splitter->setCollapsible(1, false);
    splitter->setStretchFactor(0, 65);
    splitter->setStretchFactor(1, 35);
    main_layout->addWidget(splitter);

    // 连接信号槽
    connect(btn_refresh_, &QPushButton::clicked, this, &EvidenceWidget::refresh_events);
    connect(combo_direction_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { refresh_events(); });
    connect(combo_timerange_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { refresh_events(); });
    connect(search_input_, &QLineEdit::textChanged, this, [this](const QString&) { refresh_events(); });
    connect(btn_reset_filter_, &QPushButton::clicked, this, &EvidenceWidget::reset_filters);

    connect(table_, &QTableWidget::cellDoubleClicked, this, &EvidenceWidget::on_cell_double_clicked);
    connect(table_, &QTableWidget::itemSelectionChanged, this, &EvidenceWidget::on_table_selection_changed);
}

void EvidenceWidget::reset_filters() {
    combo_direction_->blockSignals(true);
    combo_timerange_->blockSignals(true);
    search_input_->blockSignals(true);

    combo_direction_->setCurrentIndex(0);
    combo_timerange_->setCurrentIndex(0);
    search_input_->clear();

    combo_direction_->blockSignals(false);
    combo_timerange_->blockSignals(false);
    search_input_->blockSignals(false);

    refresh_events();
}

void EvidenceWidget::refresh_events() {
    int dir_idx = combo_direction_->currentIndex();
    int dir_filter = -1;
    if (dir_idx == 1) dir_filter = 0; // ENTRY
    else if (dir_idx == 2) dir_filter = 1; // EXIT

    int time_idx = combo_timerange_->currentIndex();
    int64_t start_ms = 0;
    int64_t end_ms = 0;
    QDateTime now = QDateTime::currentDateTime();
    if (time_idx == 1) {
        start_ms = QDateTime(now.date(), QTime(0, 0, 0)).toMSecsSinceEpoch();
        end_ms = now.toMSecsSinceEpoch();
    } else if (time_idx == 2) {
        start_ms = now.addDays(-3).toMSecsSinceEpoch();
        end_ms = now.toMSecsSinceEpoch();
    } else if (time_idx == 3) {
        start_ms = now.addDays(-7).toMSecsSinceEpoch();
        end_ms = now.toMSecsSinceEpoch();
    }

    QString kw = search_input_->text().trimmed();

    auto events = DatabaseManager::instance().query_filtered_events(dir_filter, start_ms, end_ms, kw, 500);
    table_->setRowCount(0);

    for (const auto& ev : events) {
        int row = table_->rowCount();
        table_->insertRow(row);
        populate_table_row(row, ev);
    }

    count_badge_->setText(QString("共 %1 条证据流水").arg(table_->rowCount()));

    if (table_->rowCount() > 0) {
        table_->selectRow(0);
        update_metadata_inspector(0);
    } else {
        update_metadata_inspector(-1);
    }
}

bool EvidenceWidget::event_matches_filter(const PassageEvent& ev) const {
    int dir_idx = combo_direction_->currentIndex();
    if (dir_idx == 1 && ev.direction != PassageDirection::ENTRY) return false;
    if (dir_idx == 2 && ev.direction != PassageDirection::EXIT) return false;

    int time_idx = combo_timerange_->currentIndex();
    if (time_idx > 0) {
        QDateTime now = QDateTime::currentDateTime();
        int64_t start_ms = 0;
        if (time_idx == 1) {
            start_ms = QDateTime(now.date(), QTime(0, 0, 0)).toMSecsSinceEpoch();
        } else if (time_idx == 2) {
            start_ms = now.addDays(-3).toMSecsSinceEpoch();
        } else if (time_idx == 3) {
            start_ms = now.addDays(-7).toMSecsSinceEpoch();
        }
        if (ev.timestamp_ms < start_ms) return false;
    }

    QString kw = search_input_->text().trimmed();
    if (!kw.isEmpty()) {
        QString full_text = QString("%1 %2 %3 %4 %5")
            .arg(ev.track_id)
            .arg(QString::fromStdString(ev.roi_name))
            .arg(QString::fromStdString(ev.clip_path))
            .arg(QString::fromStdString(ev.gender))
            .arg(QString::fromStdString(ev.age_group));
        if (!full_text.contains(kw, Qt::CaseInsensitive)) {
            return false;
        }
    }

    return true;
}

void EvidenceWidget::populate_table_row(int row, const PassageEvent& ev) {
    QString time_str = QDateTime::fromMSecsSinceEpoch(ev.timestamp_ms).toString("yyyy-MM-dd hh:mm:ss");
    QString zone_str = ev.roi_name.empty() ? QString("防区 #%1").arg(ev.roi_id) : QString::fromStdString(ev.roi_name);
    QString dir_str = (ev.direction == PassageDirection::ENTRY) ? "🟢 进入 [IN]" : "🔴 离开 [OUT]";
    QColor dir_color = (ev.direction == PassageDirection::ENTRY) ? QColor(46, 213, 115) : QColor(255, 71, 87);
    QString track_str = QString("#%1").arg(ev.track_id);
    QString gender_str = QString::fromStdString(ev.gender.empty() ? "未知" : ev.gender);
    QString age_str = QString::fromStdString(ev.age_group.empty() ? "未知" : ev.age_group);
    QString conf_str = QString("%1%").arg(QString::number(ev.confidence * 100.0f, 'f', 1));
    QString clip_str = QString::fromStdString(ev.clip_path);

    // 0. 发生时间 (居中)
    auto* item_time = new QTableWidgetItem(time_str);
    item_time->setTextAlignment(Qt::AlignCenter);
    item_time->setToolTip(time_str);

    // 1. 标定防区 (左对齐，亮青色粗体)
    auto* item_zone = new QTableWidgetItem(zone_str);
    item_zone->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    item_zone->setForeground(QColor(88, 166, 255));
    item_zone->setFont(QFont("Segoe UI", 9, QFont::Bold));
    item_zone->setToolTip(zone_str);

    // 2. 通行方向 (居中，红绿高亮)
    auto* item_dir = new QTableWidgetItem(dir_str);
    item_dir->setTextAlignment(Qt::AlignCenter);
    item_dir->setForeground(dir_color);
    item_dir->setFont(QFont("Segoe UI", 9, QFont::Bold));
    item_dir->setToolTip(dir_str);

    // 3. 目标 ID (居中)
    auto* item_id = new QTableWidgetItem(track_str);
    item_id->setTextAlignment(Qt::AlignCenter);
    item_id->setToolTip(QString("目标跟踪编号: %1").arg(track_str));

    // 4. 性别 (居中)
    auto* item_gender = new QTableWidgetItem(gender_str);
    item_gender->setTextAlignment(Qt::AlignCenter);
    item_gender->setToolTip(gender_str);

    // 5. 年龄段 (居中，支持完整展示 青年(18-35))
    auto* item_age = new QTableWidgetItem(age_str);
    item_age->setTextAlignment(Qt::AlignCenter);
    item_age->setToolTip(age_str);

    // 6. 置信度 (居中)
    auto* item_conf = new QTableWidgetItem(conf_str);
    item_conf->setTextAlignment(Qt::AlignCenter);
    item_conf->setToolTip(QString("AI 识别综合置信度: %1").arg(conf_str));

    // 7. 短视频证据切片 (左对齐，全宽自适应填充)
    auto* item_clip = new QTableWidgetItem(clip_str.isEmpty() ? "未关联本地视频切片" : clip_str);
    item_clip->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    item_clip->setToolTip(clip_str);
    if (!clip_str.isEmpty() && QFileInfo::exists(clip_str)) {
        item_clip->setForeground(QColor(63, 185, 80));
    } else {
        item_clip->setForeground(QColor(139, 148, 158));
    }

    table_->setItem(row, 0, item_time);
    table_->setItem(row, 1, item_zone);
    table_->setItem(row, 2, item_dir);
    table_->setItem(row, 3, item_id);
    table_->setItem(row, 4, item_gender);
    table_->setItem(row, 5, item_age);
    table_->setItem(row, 6, item_conf);
    table_->setItem(row, 7, item_clip);
}

void EvidenceWidget::add_new_event(const PassageEvent& ev) {
    if (!event_matches_filter(ev)) {
        return;
    }

    table_->insertRow(0);
    populate_table_row(0, ev);

    if (table_->rowCount() > 500) {
        table_->removeRow(table_->rowCount() - 1);
    }

    count_badge_->setText(QString("共 %1 条证据流水").arg(table_->rowCount()));
}

void EvidenceWidget::on_table_selection_changed() {
    int row = table_->currentRow();
    update_metadata_inspector(row);
}

void EvidenceWidget::update_metadata_inspector(int row) {
    if (row < 0 || row >= table_->rowCount()) {
        inspector_time_->setText("--");
        inspector_time_->setToolTip("");
        inspector_dir_->setText("--");
        inspector_dir_->setToolTip("");
        inspector_track_->setText("--");
        inspector_track_->setToolTip("");
        inspector_attr_->setText("--");
        inspector_attr_->setToolTip("");
        inspector_file_->setText("--");
        inspector_file_->setToolTip("");
        inspector_status_->setText("○ 未选中记录");
        inspector_status_->setStyleSheet("color: #8b949e;");
        selected_clip_path_.clear();
        btn_inspect_play_->setEnabled(false);
        return;
    }

    QString time_str = table_->item(row, 0) ? table_->item(row, 0)->text() : "--";
    QString zone_str = table_->item(row, 1) ? table_->item(row, 1)->text() : "--";
    QString dir_str = table_->item(row, 2) ? table_->item(row, 2)->text() : "--";
    QString track_id = table_->item(row, 3) ? table_->item(row, 3)->text() : "--";
    QString gender = table_->item(row, 4) ? table_->item(row, 4)->text() : "--";
    QString age = table_->item(row, 5) ? table_->item(row, 5)->text() : "--";
    QString conf = table_->item(row, 6) ? table_->item(row, 6)->text() : "--";
    QString clip_path = table_->item(row, 7) ? table_->item(row, 7)->toolTip() : "";
    if (clip_path.isEmpty() && table_->item(row, 7)) {
        clip_path = table_->item(row, 7)->text();
    }

    selected_clip_path_ = clip_path;

    inspector_time_->setText(time_str);
    inspector_time_->setToolTip(time_str);

    QString full_dir = QString("%1  │  %2").arg(zone_str).arg(dir_str);
    inspector_dir_->setText(full_dir);
    inspector_dir_->setToolTip(full_dir);

    QString full_track = QString("%1  (置信度: %2)").arg(track_id).arg(conf);
    inspector_track_->setText(full_track);
    inspector_track_->setToolTip(full_track);

    QString full_attr = QString("%1  │  %2").arg(gender).arg(age);
    inspector_attr_->setText(full_attr);
    inspector_attr_->setToolTip(full_attr);

    inspector_file_->setText(clip_path.isEmpty() ? "未关联本地视频切片" : clip_path);
    inspector_file_->setToolTip(clip_path);

    if (!clip_path.isEmpty() && QFileInfo::exists(clip_path)) {
        inspector_status_->setText("● 视频文件已就绪 (可声画回放)");
        inspector_status_->setStyleSheet("color: #3fb950; font-weight: bold;");
        btn_inspect_play_->setEnabled(true);
    } else {
        inspector_status_->setText("○ 切片不存在或正在后台生成");
        inspector_status_->setStyleSheet("color: #ffa502; font-weight: bold;");
        btn_inspect_play_->setEnabled(false);
    }
}

void EvidenceWidget::on_cell_double_clicked(int row, int) {
    if (row < 0 || row >= table_->rowCount()) return;
    auto* clip_item = table_->item(row, 7);
    if (!clip_item) return;

    QString file_path = clip_item->toolTip().isEmpty() ? clip_item->text() : clip_item->toolTip();
    play_clip(file_path);
}

void EvidenceWidget::play_clip(const QString& file_path) {
    if (file_path.isEmpty() || !QFileInfo::exists(file_path)) {
        QMessageBox::information(this, "提示", QString("切片文件不存在或正在异步落盘写入中:\n%1").arg(file_path));
        return;
    }
    player_->load_media(file_path);
    player_->play();
}

