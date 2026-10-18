#include "StudioWidget.hpp"
#include "ui/VideoCanvas.hpp"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QScrollArea>

StudioWidget::StudioWidget(QWidget* parent) : QWidget(parent) {
    setup_ui();
}

void StudioWidget::setup_ui() {
    auto* main_layout = new QHBoxLayout(this);
    main_layout->setContentsMargins(14, 14, 14, 14);
    main_layout->setSpacing(14);

    // 1. 左侧大标定交互画布
    canvas_ = new VideoCanvas(this);
    canvas_->set_read_only(false);
    canvas_->setStyleSheet("border: 1px solid #30363d; border-radius: 8px;");
    main_layout->addWidget(canvas_, 4);

    // 2. 右侧标定控制面板 (支持垂直自适应滚动)
    auto* right_scroll = new QScrollArea(this);
    right_scroll->setFixedWidth(330);
    right_scroll->setWidgetResizable(true);
    right_scroll->setFrameShape(QFrame::NoFrame);
    right_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    right_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto* right_panel = new QWidget(right_scroll);
    auto* right_vbox = new QVBoxLayout(right_panel);
    right_vbox->setContentsMargins(0, 0, 6, 0);
    right_vbox->setSpacing(12);

    auto* tool_group = new QGroupBox("空间几何标定工具", this);
    auto* tool_vbox = new QVBoxLayout(tool_group);
    tool_vbox->setSpacing(10);

    // 0. 多防区 (Multi-ROI) 切换与管理卡片
    auto* roi_mgmt_card = new QWidget(this);
    roi_mgmt_card->setStyleSheet("background-color: #0d1117; border: 1px solid #21262d; border-radius: 6px; padding: 8px;");
    auto* roi_mgmt_layout = new QVBoxLayout(roi_mgmt_card);
    roi_mgmt_layout->setSpacing(6);

    auto* roi_mgmt_title = new QLabel("🏷️  防区选择与管理 (Multi-ROI)", roi_mgmt_card);
    roi_mgmt_title->setStyleSheet("color: #58a6ff; font-weight: bold; font-size: 12px;");
    roi_mgmt_layout->addWidget(roi_mgmt_title);

    auto* roi_sel_row = new QHBoxLayout();
    roi_combo_ = new QComboBox(roi_mgmt_card);
    roi_combo_->setStyleSheet("background-color: #161b22; color: #58a6ff; font-weight: bold; padding: 4px; border: 1px solid #30363d; border-radius: 4px;");
    roi_sel_row->addWidget(roi_combo_, 1);

    btn_add_roi_ = new QPushButton("➕ 新增", roi_mgmt_card);
    btn_add_roi_->setStyleSheet("background-color: #238636; color: white; font-weight: bold; padding: 4px 8px; border-radius: 4px;");
    btn_del_roi_ = new QPushButton("🗑️ 删除", roi_mgmt_card);
    btn_del_roi_->setStyleSheet("background-color: #da3633; color: white; font-weight: bold; padding: 4px 8px; border-radius: 4px;");
    roi_sel_row->addWidget(btn_add_roi_);
    roi_sel_row->addWidget(btn_del_roi_);
    roi_mgmt_layout->addLayout(roi_sel_row);

    auto* name_row = new QHBoxLayout();
    auto* name_lbl = new QLabel("防区名称:", roi_mgmt_card);
    name_lbl->setStyleSheet("color: #8b949e; font-size: 11px;");
    roi_name_edit_ = new QLineEdit("A门主通道", roi_mgmt_card);
    roi_name_edit_->setStyleSheet("background-color: #161b22; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 3px 6px;");
    name_row->addWidget(name_lbl);
    name_row->addWidget(roi_name_edit_, 1);
    roi_mgmt_layout->addLayout(name_row);

    tool_vbox->addWidget(roi_mgmt_card);

    auto* op_card = new QWidget(this);
    op_card->setStyleSheet("background-color: #0d1117; border: 1px solid #21262d; border-radius: 6px; padding: 10px;");
    auto* op_layout = new QVBoxLayout(op_card);
    op_layout->setContentsMargins(2, 2, 2, 2);

    auto* tip_title = new QLabel("📌 标定交互指引", op_card);
    tip_title->setStyleSheet("color: #58a6ff; font-weight: bold; font-size: 12px;");
    auto* op_label = new QLabel(
        "1. 在防区管理中选择或新建防区；\n"
        "2. 点击 [重新打点]，在画面依次单击落点，双击闭合；\n"
        "3. 拖拽红点微调单点，内部按住拖拽整体平移。",
        op_card
    );
    op_label->setWordWrap(true);
    op_label->setStyleSheet("color: #8b949e; font-size: 11px; line-height: 150%;");
    op_layout->addWidget(tip_title);
    op_layout->addWidget(op_label);
    tool_vbox->addWidget(op_card);

    btn_draw_ = new QPushButton("✏️  重新打点当前防区", this);
    btn_draw_->setObjectName("btn_accent");
    btn_draw_->setFixedHeight(38);
    tool_vbox->addWidget(btn_draw_);

    btn_reset_ = new QPushButton("🗑️  清空当前防区顶点", this);
    btn_reset_->setFixedHeight(32);
    tool_vbox->addWidget(btn_reset_);

    btn_invert_dir_ = new QPushButton("🔄  反转进出判定朝向", this);
    btn_invert_dir_->setFixedHeight(32);
    btn_invert_dir_->setStyleSheet(
        "QPushButton { background-color: #21262d; color: #58a6ff; border: 1px solid #30363d; border-radius: 4px; font-weight: 600; font-size: 12px; } "
        "QPushButton:hover { background-color: #30363d; border-color: #58a6ff; color: #ffffff; }"
    );
    tool_vbox->addWidget(btn_invert_dir_);

    btn_preset_default_ = new QPushButton("🎯  一键预置默认通道防区", this);
    btn_preset_default_->setFixedHeight(32);
    btn_preset_default_->setStyleSheet(
        "QPushButton { background-color: #21262d; color: #7ee787; border: 1px solid #30363d; border-radius: 4px; font-weight: 600; font-size: 12px; } "
        "QPushButton:hover { background-color: #30363d; border-color: #7ee787; color: #ffffff; }"
    );
    tool_vbox->addWidget(btn_preset_default_);

    status_label_ = new QLabel("● 状态: 空闲 / 等待标定", this);
    status_label_->setStyleSheet("color: #ffa502; font-weight: bold; background: #21262d; padding: 6px; border-radius: 4px;");
    tool_vbox->addWidget(status_label_);

    tool_vbox->addSpacing(10);

    // 防抖参数微调
    auto* param_group = new QGroupBox("滞后防抖状态机参数", this);
    auto* param_vbox = new QVBoxLayout(param_group);
    param_vbox->setSpacing(8);

    buffer_val_label_ = new QLabel("几何缓冲带半径: 20 px", this);
    buffer_val_label_->setStyleSheet("color: #c9d1d9; font-size: 12px;");
    buffer_slider_ = new QSlider(Qt::Horizontal, this);
    buffer_slider_->setRange(10, 40);
    buffer_slider_->setValue(20);
    param_vbox->addWidget(buffer_val_label_);
    param_vbox->addWidget(buffer_slider_);

    confirm_val_label_ = new QLabel("连续穿越确认帧: 4 帧", this);
    confirm_val_label_->setStyleSheet("color: #c9d1d9; font-size: 12px;");
    confirm_slider_ = new QSlider(Qt::Horizontal, this);
    confirm_slider_->setRange(2, 8);
    confirm_slider_->setValue(4);
    param_vbox->addWidget(confirm_val_label_);
    param_vbox->addWidget(confirm_slider_);

    tool_vbox->addWidget(param_group);
    tool_vbox->addStretch();

    // 底部保存控制
    auto* btn_box = new QHBoxLayout();
    btn_cancel_ = new QPushButton("取消修改", this);
    btn_cancel_->setFixedHeight(36);

    btn_apply_ = new QPushButton("✓  立即应用全部防区", this);
    btn_apply_->setObjectName("btn_primary");
    btn_apply_->setFixedHeight(36);

    btn_box->addWidget(btn_cancel_);
    btn_box->addWidget(btn_apply_);
    tool_vbox->addLayout(btn_box);

    right_vbox->addWidget(tool_group);
    right_scroll->setWidget(right_panel);
    main_layout->addWidget(right_scroll, 0);

    // 连接信号槽
    connect(btn_draw_, &QPushButton::clicked, this, &StudioWidget::on_btn_start_draw);
    connect(btn_reset_, &QPushButton::clicked, this, &StudioWidget::on_btn_reset);
    connect(btn_apply_, &QPushButton::clicked, this, &StudioWidget::on_btn_apply);
    connect(btn_cancel_, &QPushButton::clicked, this, &StudioWidget::on_btn_cancel);
    connect(canvas_, &VideoCanvas::polygon_status_changed, this, &StudioWidget::on_polygon_status);

    connect(roi_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (idx >= 0) {
            canvas_->set_active_roi_index(idx);
            refresh_roi_combo();
        }
    });

    connect(roi_name_edit_, &QLineEdit::textEdited, this, [this](const QString& text) {
        canvas_->set_active_roi_name(text.toStdString());
        int cur = roi_combo_->currentIndex();
        if (cur >= 0 && cur < roi_combo_->count()) {
            roi_combo_->setItemText(cur, QString("[%1] %2").arg(cur + 1).arg(text));
        }
    });

    connect(btn_add_roi_, &QPushButton::clicked, this, [this]() {
        const char* colors[] = {"#f1e05a", "#bc8cff", "#3fb950", "#ff7b72", "#58a6ff"};
        auto rois = canvas_->get_rois();
        int new_id = static_cast<int>(rois.size()) + 1;
        std::string col = colors[(new_id - 1) % 5];
        std::string def_name = QString("防区 %1").arg(new_id).toStdString();

        RoiConfig new_cfg;
        new_cfg.roi_id = new_id;
        new_cfg.name = def_name;
        new_cfg.color_hex = col;
        new_cfg.ref_width = canvas_->video_width() > 0 ? canvas_->video_width() : 1920;
        new_cfg.ref_height = canvas_->video_height() > 0 ? canvas_->video_height() : 1080;
        new_cfg.buffer_distance = 10.0f;
        new_cfg.min_confirm_frames = 2;
        new_cfg.is_valid = false;

        canvas_->add_roi(new_cfg);
        refresh_roi_combo();
        canvas_->start_drawing();
        status_label_->setText(QString("● 已添加【%1】: 请在画面上依次打点，双击闭合").arg(QString::fromStdString(def_name)));
        status_label_->setStyleSheet("color: #58a6ff; font-weight: bold; background: #21262d; padding: 6px; border-radius: 4px;");
    });

    connect(btn_del_roi_, &QPushButton::clicked, this, [this]() {
        canvas_->remove_active_roi();
        refresh_roi_combo();
        status_label_->setText("● 状态: 已移除当前防区");
        status_label_->setStyleSheet("color: #ff7675; font-weight: bold; background: #21262d; padding: 6px; border-radius: 4px;");
    });

    connect(btn_invert_dir_, &QPushButton::clicked, this, [this]() {
        invert_direction_ = !invert_direction_;
        canvas_->set_active_roi_invert_direction(invert_direction_);
        status_label_->setText(invert_direction_ ? "● 判定朝向: 已反转 (向外为进)" : "● 判定朝向: 默认方向 (向内为进)");
        status_label_->setStyleSheet("color: #58a6ff; font-weight: bold; background: #21262d; padding: 6px; border-radius: 4px;");
    });

    connect(btn_preset_default_, &QPushButton::clicked, this, &StudioWidget::on_btn_preset_default);

    connect(buffer_slider_, &QSlider::valueChanged, this, [this](int val) {
        buffer_val_label_->setText(QString("几何缓冲带半径: %1 px").arg(val));
    });

    connect(confirm_slider_, &QSlider::valueChanged, this, [this](int val) {
        confirm_val_label_->setText(QString("连续穿越确认帧: %1 帧").arg(val));
    });

    connect(canvas_, &VideoCanvas::roi_applied, this, [this](const RoiConfig& roi) {
        emit roi_saved(roi);
    });

    connect(canvas_, &VideoCanvas::rois_applied, this, [this](const std::vector<RoiConfig>& rois) {
        emit rois_saved(rois);
    });
}

void StudioWidget::refresh_roi_combo() {
    if (!roi_combo_ || !canvas_) return;
    roi_combo_->blockSignals(true);
    roi_combo_->clear();
    auto rois = canvas_->get_rois();
    for (size_t i = 0; i < rois.size(); ++i) {
        roi_combo_->addItem(QString("[%1] %2").arg(i + 1).arg(QString::fromStdString(rois[i].name)));
    }
    int cur_idx = canvas_->active_roi_index();
    if (cur_idx >= 0 && cur_idx < roi_combo_->count()) {
        roi_combo_->setCurrentIndex(cur_idx);
    }
    roi_combo_->blockSignals(false);

    RoiConfig cur = canvas_->get_roi();
    if (roi_name_edit_) {
        roi_name_edit_->setText(QString::fromStdString(cur.name));
    }
    invert_direction_ = cur.invert_direction;
    if (buffer_slider_) {
        buffer_slider_->setValue(static_cast<int>(cur.buffer_distance));
    }
    if (confirm_slider_) {
        confirm_slider_->setValue(cur.min_confirm_frames);
    }
    if (status_label_) {
        if (cur.is_valid && cur.vertices.size() >= 3) {
            status_label_->setText(QString("● 当前防区【%1】: %2 个顶点 (朝向: %3)")
                .arg(QString::fromStdString(cur.name))
                .arg(cur.vertices.size())
                .arg(cur.invert_direction ? "反转" : "常规"));
            status_label_->setStyleSheet("color: #2ed573; font-weight: bold; background: #21262d; padding: 6px; border-radius: 4px;");
        } else {
            status_label_->setText(QString("● 当前防区【%1】: 待标定").arg(QString::fromStdString(cur.name)));
            status_label_->setStyleSheet("color: #ffa502; font-weight: bold; background: #21262d; padding: 6px; border-radius: 4px;");
        }
    }
}

void StudioWidget::update_frame(const QImage& frame, const std::vector<TrackObject>& tracks) {
    if (canvas_) {
        canvas_->update_frame(frame, tracks);
    }
}

void StudioWidget::set_roi(const RoiConfig& roi) {
    set_rois({roi});
}

void StudioWidget::set_rois(const std::vector<RoiConfig>& rois) {
    if (canvas_) {
        canvas_->set_rois(rois);
    }
    refresh_roi_combo();
}

void StudioWidget::on_btn_start_draw() {
    canvas_->start_drawing();
    status_label_->setText("● 正在打点: 单击加点，双击闭合");
    status_label_->setStyleSheet("color: #58a6ff; font-weight: bold; background: #21262d; padding: 6px; border-radius: 4px;");
}

void StudioWidget::on_btn_reset() {
    canvas_->reset_roi();
    status_label_->setText("● 状态: 已清空区域");
    status_label_->setStyleSheet("color: #ff7675; font-weight: bold; background: #21262d; padding: 6px; border-radius: 4px;");
}

void StudioWidget::on_btn_apply() {
    canvas_->set_active_roi_buffer_dist(static_cast<float>(buffer_slider_->value()));
    canvas_->set_active_roi_confirm_frames(confirm_slider_->value());
    canvas_->set_active_roi_invert_direction(invert_direction_);
    if (roi_name_edit_) {
        canvas_->set_active_roi_name(roi_name_edit_->text().toStdString());
    }
    canvas_->apply_changes();
    auto all_rois = canvas_->get_rois();
    emit rois_saved(all_rois);
    emit roi_saved(canvas_->get_roi());
    refresh_roi_combo();
    status_label_->setText(QString("● 状态: 全部 %1 个防区已成功生效并存盘").arg(all_rois.size()));
    status_label_->setStyleSheet("color: #2ed573; font-weight: bold; background: #21262d; padding: 6px; border-radius: 4px;");
}

void StudioWidget::on_btn_cancel() {
    canvas_->cancel_changes();
    refresh_roi_combo();
    status_label_->setText("● 状态: 已恢复上次配置");
    status_label_->setStyleSheet("color: #8b949e; font-weight: bold; background: #21262d; padding: 6px; border-radius: 4px;");
}

void StudioWidget::on_btn_preset_default() {
    int w = canvas_->video_width() > 0 ? canvas_->video_width() : 1920;
    int h = canvas_->video_height() > 0 ? canvas_->video_height() : 1080;
    RoiConfig def_roi = make_default_corridor_roi(w, h);
    set_roi(def_roi);
    status_label_->setText("● 状态: 已加载默认通道防区，点击【立即应用生效】即可存盘");
    status_label_->setStyleSheet("color: #58a6ff; font-weight: bold; background: #21262d; padding: 6px; border-radius: 4px;");
}

void StudioWidget::on_polygon_status(bool is_valid, int vertex_count) {
    if (vertex_count < 3) {
        status_label_->setText(QString("● 打点中: 已落 %1 点").arg(vertex_count));
        status_label_->setStyleSheet("color: #58a6ff; font-weight: bold; background: #21262d; padding: 6px; border-radius: 4px;");
    } else if (is_valid) {
        status_label_->setText(QString("● 合法多边形: %1 个顶点 (可拖拽微调)").arg(vertex_count));
        status_label_->setStyleSheet("color: #2ed573; font-weight: bold; background: #21262d; padding: 6px; border-radius: 4px;");
    } else {
        status_label_->setText("● 非法多边形: 请避免线段自相交");
        status_label_->setStyleSheet("color: #ff4757; font-weight: bold; background: #21262d; padding: 6px; border-radius: 4px;");
    }
}
