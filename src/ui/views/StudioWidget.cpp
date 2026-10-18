#include "StudioWidget.hpp"
#include "ui/VideoCanvas.hpp"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>

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

    // 2. 右侧标定控制面板
    auto* right_panel = new QWidget(this);
    right_panel->setFixedWidth(310);
    auto* right_vbox = new QVBoxLayout(right_panel);
    right_vbox->setContentsMargins(0, 0, 0, 0);
    right_vbox->setSpacing(12);

    auto* tool_group = new QGroupBox("空间几何标定工具", this);
    auto* tool_vbox = new QVBoxLayout(tool_group);
    tool_vbox->setSpacing(10);

    auto* op_card = new QWidget(this);
    op_card->setStyleSheet("background-color: #0d1117; border: 1px solid #21262d; border-radius: 6px; padding: 10px;");
    auto* op_layout = new QVBoxLayout(op_card);
    op_layout->setContentsMargins(2, 2, 2, 2);

    auto* tip_title = new QLabel("📌 标定交互指引", op_card);
    tip_title->setStyleSheet("color: #58a6ff; font-weight: bold; font-size: 12px;");
    auto* op_label = new QLabel(
        "1. 点击 [新建多边形]，在画面上依次单击打下顶点；\n"
        "2. 在最后一个点双击，自动闭合生成 N 边形；\n"
        "3. 拖拽红色圆点微调单点，内部按住拖拽整体平移。",
        op_card
    );
    op_label->setWordWrap(true);
    op_label->setStyleSheet("color: #8b949e; font-size: 11px; line-height: 150%;");
    op_layout->addWidget(tip_title);
    op_layout->addWidget(op_label);
    tool_vbox->addWidget(op_card);

    btn_draw_ = new QPushButton("✏️  新建多边形 (N边形)", this);
    btn_draw_->setObjectName("btn_accent");
    btn_draw_->setFixedHeight(38);
    tool_vbox->addWidget(btn_draw_);

    btn_reset_ = new QPushButton("🗑️  清空当前区域", this);
    btn_reset_->setFixedHeight(32);
    tool_vbox->addWidget(btn_reset_);

    btn_invert_dir_ = new QPushButton("🔄  反转进出判定朝向", this);
    btn_invert_dir_->setFixedHeight(32);
    btn_invert_dir_->setStyleSheet(
        "QPushButton { background-color: #21262d; color: #58a6ff; border: 1px solid #30363d; border-radius: 4px; font-weight: 600; font-size: 12px; } "
        "QPushButton:hover { background-color: #30363d; border-color: #58a6ff; color: #ffffff; }"
    );
    tool_vbox->addWidget(btn_invert_dir_);

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

    btn_apply_ = new QPushButton("✓  立即应用生效", this);
    btn_apply_->setObjectName("btn_primary");
    btn_apply_->setFixedHeight(36);

    btn_box->addWidget(btn_cancel_);
    btn_box->addWidget(btn_apply_);
    tool_vbox->addLayout(btn_box);

    right_vbox->addWidget(tool_group);
    main_layout->addWidget(right_panel, 1);

    // 连接信号槽
    connect(btn_draw_, &QPushButton::clicked, this, &StudioWidget::on_btn_start_draw);
    connect(btn_reset_, &QPushButton::clicked, this, &StudioWidget::on_btn_reset);
    connect(btn_apply_, &QPushButton::clicked, this, &StudioWidget::on_btn_apply);
    connect(btn_cancel_, &QPushButton::clicked, this, &StudioWidget::on_btn_cancel);
    connect(canvas_, &VideoCanvas::polygon_status_changed, this, &StudioWidget::on_polygon_status);

    connect(btn_invert_dir_, &QPushButton::clicked, this, [this]() {
        RoiConfig cfg = canvas_->get_roi();
        invert_direction_ = !invert_direction_;
        cfg.invert_direction = invert_direction_;
        canvas_->set_roi(cfg);
        status_label_->setText(invert_direction_ ? "● 判定朝向: 已反转 (向外为进)" : "● 判定朝向: 默认方向 (向内为进)");
        status_label_->setStyleSheet("color: #58a6ff; font-weight: bold; background: #21262d; padding: 6px; border-radius: 4px;");
    });

    connect(buffer_slider_, &QSlider::valueChanged, this, [this](int val) {
        buffer_val_label_->setText(QString("几何缓冲带半径: %1 px").arg(val));
    });

    connect(confirm_slider_, &QSlider::valueChanged, this, [this](int val) {
        confirm_val_label_->setText(QString("连续穿越确认帧: %1 帧").arg(val));
    });

    connect(canvas_, &VideoCanvas::roi_applied, this, [this](const RoiConfig& roi) {
        emit roi_saved(roi);
    });
}

void StudioWidget::update_frame(const QImage& frame, const std::vector<TrackObject>& tracks) {
    if (canvas_) {
        canvas_->update_frame(frame, tracks);
    }
}

void StudioWidget::set_roi(const RoiConfig& roi) {
    if (canvas_) {
        canvas_->set_roi(roi);
    }
    invert_direction_ = roi.invert_direction;
    if (buffer_slider_) {
        buffer_slider_->setValue(static_cast<int>(roi.buffer_distance));
    }
    if (confirm_slider_) {
        confirm_slider_->setValue(roi.min_confirm_frames);
    }
    if (status_label_) {
        if (roi.is_valid && roi.vertices.size() >= 3) {
            status_label_->setText(QString("● 当前已载入历史标定: %1 个顶点 (朝向: %2)")
                .arg(roi.vertices.size())
                .arg(roi.invert_direction ? "反转" : "常规"));
            status_label_->setStyleSheet("color: #2ed573; font-weight: bold; background: #21262d; padding: 6px; border-radius: 4px;");
        }
    }
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
    RoiConfig cfg = canvas_->get_roi();
    cfg.buffer_distance = static_cast<float>(buffer_slider_->value());
    cfg.min_confirm_frames = confirm_slider_->value();
    cfg.invert_direction = invert_direction_;
    canvas_->set_roi(cfg);
    canvas_->apply_changes();
    status_label_->setText("● 状态: 配置已成功应用并存盘");
    status_label_->setStyleSheet("color: #2ed573; font-weight: bold; background: #21262d; padding: 6px; border-radius: 4px;");
}

void StudioWidget::on_btn_cancel() {
    canvas_->cancel_changes();
    status_label_->setText("● 状态: 已恢复上次配置");
    status_label_->setStyleSheet("color: #8b949e; font-weight: bold; background: #21262d; padding: 6px; border-radius: 4px;");
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
