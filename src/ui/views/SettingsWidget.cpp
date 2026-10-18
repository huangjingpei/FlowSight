#include "SettingsWidget.hpp"
#include "storage/ConfigManager.hpp"
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

    auto& cfg = ConfigManager::instance();

    // 1. 视频源设置
    auto* src_group = new QGroupBox("视频输入源设置", this);
    auto* src_layout = new QHBoxLayout(src_group);

    video_input_edit_ = new QLineEdit(cfg.get_video_source(), this);
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
    retention_spin_->setValue(cfg.get_clip_retention_days());
    storage_layout->addWidget(retention_spin_);
    storage_layout->addWidget(new QLabel("天 (过期切片自动静默清理，保护磁盘空间)", this));
    storage_layout->addStretch();
    main_layout->addWidget(storage_group);

    // 3. 客流拥挤度超载告警设置 (Crowd Surge Alert)
    auto* surge_group = new QGroupBox("客流拥挤度超载告警设置 (Crowd Surge Alert)", this);
    auto* surge_layout = new QVBoxLayout(surge_group);

    auto* surge_row1 = new QHBoxLayout();
    surge_enabled_chk_ = new QCheckBox("启用区域人员超载预警 (防踩踏/负荷风控)", this);
    surge_enabled_chk_->setChecked(cfg.get_crowd_surge_enabled());
    surge_enabled_chk_->setStyleSheet("color: #f85149; font-weight: bold;");
    surge_row1->addWidget(surge_enabled_chk_);

    surge_row1->addSpacing(20);
    surge_row1->addWidget(new QLabel("人员超载报警阈值:", this));
    surge_threshold_spin_ = new QSpinBox(this);
    surge_threshold_spin_->setRange(1, 200);
    surge_threshold_spin_->setValue(cfg.get_crowd_surge_threshold());
    surge_threshold_spin_->setSuffix(" 人");
    surge_row1->addWidget(surge_threshold_spin_);

    surge_row1->addSpacing(20);
    surge_beep_chk_ = new QCheckBox("触发系统声音提示 (Beep)", this);
    surge_beep_chk_->setChecked(cfg.get_crowd_surge_beep());
    surge_row1->addWidget(surge_beep_chk_);
    surge_row1->addStretch();

    surge_layout->addLayout(surge_row1);

    auto* surge_hint = new QLabel(
        "• 告警机制: 当监控区域内活跃人数达到或超过设定阈值时，视频画布将触发「红色赛博呼吸律动边框」与「悬浮发光警标」；\n"
        "• 解除机制: 人数连续多帧低于预警线后平滑恢复常态冷色调监控，杜绝边界临界值抖动。",
        this
    );
    surge_hint->setStyleSheet("color: #8b949e; font-size: 12px; margin-top: 4px;");
    surge_layout->addWidget(surge_hint);
    main_layout->addWidget(surge_group);

    // 4. AI 硬件信息
    auto* hw_group = new QGroupBox("AI 推理硬件环境", this);
    auto* hw_layout = new QVBoxLayout(hw_group);

    hardware_info_label_ = new QLabel(
        "• 推理框架: ONNX Runtime C++ API (v1.24)\n"
        "• 目标平台: Windows 10/11 x64\n"
        "• 检测模型: YOLOv8n (640x640 FP32, Person Class Only)\n"
        "• 多目标跟踪: ByteTrack C++ (Kalman Filter + IoU)\n"
        "• 数据库模式: SQLite 3 (WAL 并发模式已激活)\n"
        "• 配置存储: configs/flowsight.ini (持久化已激活)",
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
    auto& cfg = ConfigManager::instance();
    cfg.set_video_source(video_input_edit_->text());
    cfg.set_clip_retention_days(retention_spin_->value());

    cfg.set_crowd_surge_enabled(surge_enabled_chk_->isChecked());
    cfg.set_crowd_surge_threshold(surge_threshold_spin_->value());
    cfg.set_crowd_surge_beep(surge_beep_chk_->isChecked());

    cfg.save();

    emit video_source_changed(video_input_edit_->text());
    QMessageBox::information(this, "成功", "设置已保存至 configs/flowsight.ini！已实时应用并持久化保存。");
}
