#include "SettingsWidget.hpp"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>

SettingsWidget::SettingsWidget(QWidget* parent) : QWidget(parent) {
    setup_ui();
}

void SettingsWidget::setup_ui() {
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(12, 12, 12, 12);
    main_layout->setSpacing(12);

    // 1. 视频源设置
    auto* src_group = new QGroupBox("视频输入源设置", this);
    auto* src_layout = new QHBoxLayout(src_group);

    video_input_edit_ = new QLineEdit("configs/test_corridor.mp4", this);
    btn_browse_ = new QPushButton("浏览本地文件...", this);
    src_layout->addWidget(video_input_edit_, 1);
    src_layout->addWidget(btn_browse_);
    main_layout->addWidget(src_group);

    // 2. 存储与短视频切片管理
    auto* storage_group = new QGroupBox("存储与磁盘保护策略", this);
    auto* storage_layout = new QHBoxLayout(storage_group);

    storage_layout->addWidget(new QLabel("短视频切片保留天数:", this));
    retention_spin_ = new QSpinBox(this);
    retention_spin_->setRange(1, 365);
    retention_spin_->setValue(30);
    storage_layout->addWidget(retention_spin_);
    storage_layout->addWidget(new QLabel("天 (过期切片自动静默清理，保护磁盘空间)", this));
    storage_layout->addStretch();
    main_layout->addWidget(storage_group);

    // 3. AI 硬件信息
    auto* hw_group = new QGroupBox("AI 推理硬件环境", this);
    auto* hw_layout = new QVBoxLayout(hw_group);

    hardware_info_label_ = new QLabel(
        "• 推理框架: ONNX Runtime C++ API (v1.24)\n"
        "• 目标平台: Windows 10/11 x64\n"
        "• 检测模型: YOLOv8n (640x640 FP32, Person Class Only)\n"
        "• 多目标跟踪: ByteTrack C++ (Kalman Filter + IoU)\n"
        "• 数据库模式: SQLite 3 (WAL 并发模式已激活)",
        this
    );
    hardware_info_label_->setStyleSheet("color: #74b9ff; font-size: 13px; line-height: 160%;");
    hw_layout->addWidget(hardware_info_label_);
    main_layout->addWidget(hw_group);

    main_layout->addStretch();

    btn_save_ = new QPushButton("保存并应用设置", this);
    btn_save_->setStyleSheet("padding: 8px 16px; background: #0984e3; color: white; font-weight: bold; border-radius: 4px;");
    main_layout->addWidget(btn_save_, 0, Qt::AlignRight);

    connect(btn_browse_, &QPushButton::clicked, this, &SettingsWidget::on_btn_browse);
    connect(btn_save_, &QPushButton::clicked, this, &SettingsWidget::on_btn_save);
}

void SettingsWidget::on_btn_browse() {
    QString file = QFileDialog::getOpenFileName(this, "选择测试视频", "", "Video Files (*.mp4 *.avi *.mkv)");
    if (!file.isEmpty()) {
        video_input_edit_->setText(file);
    }
}

void SettingsWidget::on_btn_save() {
    emit video_source_changed(video_input_edit_->text());
    QMessageBox::information(this, "成功", "设置已保存！视频源变更将在下一次启动或重载时生效。");
}
