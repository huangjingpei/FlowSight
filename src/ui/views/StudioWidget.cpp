#include "StudioWidget.hpp"
#include "ui/VideoCanvas.hpp"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QMessageBox>

StudioWidget::StudioWidget(QWidget* parent) : QWidget(parent) {
    setup_ui();
}

void StudioWidget::setup_ui() {
    auto* main_layout = new QHBoxLayout(this);
    main_layout->setContentsMargins(8, 8, 8, 8);
    main_layout->setSpacing(8);

    // 1. 左侧大标定交互画布
    canvas_ = new VideoCanvas(this);
    canvas_->set_read_only(false);
    main_layout->addWidget(canvas_, 4);

    // 2. 右侧标定工具箱
    auto* right_panel = new QWidget(this);
    right_panel->setFixedWidth(280);
    auto* right_vbox = new QVBoxLayout(right_panel);
    right_vbox->setContentsMargins(0, 0, 0, 0);

    auto* tool_group = new QGroupBox("空间标定工作台", this);
    auto* tool_vbox = new QVBoxLayout(tool_group);

    // 绘制操作
    auto* op_label = new QLabel("操作指引: 点击[新建多边形]后，在画面上单击添加顶点，双击闭合多边形；拖拽红色圆点可微调位置，多边形内部按住拖拽可整体平移。", this);
    op_label->setWordWrap(true);
    op_label->setStyleSheet("color: #bbb; font-size: 12px; margin-bottom: 10px;");
    tool_vbox->addWidget(op_label);

    btn_draw_ = new QPushButton("新建多边形 (N边形)", this);
    btn_draw_->setStyleSheet("padding: 8px; font-weight: bold; background: #0984e3; color: white; border-radius: 4px;");
    tool_vbox->addWidget(btn_draw_);

    btn_reset_ = new QPushButton("清空重置", this);
    btn_reset_->setStyleSheet("padding: 6px;");
    tool_vbox->addWidget(btn_reset_);

    status_label_ = new QLabel("状态: 等待标定", this);
    status_label_->setStyleSheet("color: #ffeaa7; font-weight: bold; margin-top: 5px;");
    tool_vbox->addWidget(status_label_);

    tool_vbox->addSpacing(15);

    // 参数微调
    auto* param_group = new QGroupBox("防抖状态机参数", this);
    auto* param_vbox = new QVBoxLayout(param_group);

    buffer_val_label_ = new QLabel("几何缓冲带: 20 px", this);
    buffer_slider_ = new QSlider(Qt::Horizontal, this);
    buffer_slider_->setRange(10, 40);
    buffer_slider_->setValue(20);
    param_vbox->addWidget(buffer_val_label_);
    param_vbox->addWidget(buffer_slider_);

    confirm_val_label_ = new QLabel("连续确认帧: 4 帧", this);
    confirm_slider_ = new QSlider(Qt::Horizontal, this);
    confirm_slider_->setRange(2, 8);
    confirm_slider_->setValue(4);
    param_vbox->addWidget(confirm_val_label_);
    param_vbox->addWidget(confirm_slider_);

    tool_vbox->addWidget(param_group);
    tool_vbox->addStretch();

    // 生效与取消按钮
    auto* btn_box = new QHBoxLayout();
    btn_cancel_ = new QPushButton("取消修改", this);
    btn_cancel_->setStyleSheet("padding: 8px;");
    btn_apply_ = new QPushButton("立即应用生效", this);
    btn_apply_->setStyleSheet("padding: 8px; font-weight: bold; background: #00b894; color: white; border-radius: 4px;");

    btn_box->addWidget(btn_cancel_);
    btn_box->addWidget(btn_apply_);
    tool_vbox->addLayout(btn_box);

    right_vbox->addWidget(tool_group);
    main_layout->addWidget(right_panel, 1);

    // 信号槽连接
    connect(btn_draw_, &QPushButton::clicked, this, &StudioWidget::on_btn_start_draw);
    connect(btn_reset_, &QPushButton::clicked, this, &StudioWidget::on_btn_reset);
    connect(btn_apply_, &QPushButton::clicked, this, &StudioWidget::on_btn_apply);
    connect(btn_cancel_, &QPushButton::clicked, this, &StudioWidget::on_btn_cancel);
    connect(canvas_, &VideoCanvas::polygon_status_changed, this, &StudioWidget::on_polygon_status);

    connect(buffer_slider_, &QSlider::valueChanged, this, [this](int val) {
        buffer_val_label_->setText(QString("几何缓冲带: %1 px").arg(val));
    });

    connect(confirm_slider_, &QSlider::valueChanged, this, [this](int val) {
        confirm_val_label_->setText(QString("连续确认帧: %1 帧").arg(val));
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
}

void StudioWidget::on_btn_start_draw() {
    canvas_->start_drawing();
    status_label_->setText("状态: 单击打点，双击闭合");
}

void StudioWidget::on_btn_reset() {
    canvas_->reset_roi();
    status_label_->setText("状态: 已清空");
}

void StudioWidget::on_btn_apply() {
    RoiConfig cfg = canvas_->get_roi();
    cfg.buffer_distance = static_cast<float>(buffer_slider_->value());
    cfg.min_confirm_frames = confirm_slider_->value();
    canvas_->set_roi(cfg);
    canvas_->apply_changes();
    status_label_->setText("状态: 已应用生效");
}

void StudioWidget::on_btn_cancel() {
    canvas_->cancel_changes();
    status_label_->setText("状态: 已取消修改");
}

void StudioWidget::on_polygon_status(bool is_valid, int vertex_count) {
    if (vertex_count < 3) {
        status_label_->setText(QString("状态: 正在打点 (%1点)").arg(vertex_count));
    } else if (is_valid) {
        status_label_->setText(QString("状态: 合法 %1 边形 (可拖拽微调)").arg(vertex_count));
    } else {
        status_label_->setText("状态: 非法多边形 (请避免自交)");
    }
}
