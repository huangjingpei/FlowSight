#include "SettingsWidget.hpp"
#include "storage/ConfigManager.hpp"
#include "network/WebhookPusher.hpp"
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

    // 4. Webhook 云端推送总线设置 (HTTP Webhook Bus)
    auto* webhook_group = new QGroupBox("云端 Webhook 事件推送总线 (HTTP Webhook Bus)", this);
    auto* webhook_layout = new QVBoxLayout(webhook_group);

    auto* webhook_row1 = new QHBoxLayout();
    webhook_enabled_chk_ = new QCheckBox("启用 Webhook 实时异步推送 (通行事件与超载告警)", this);
    webhook_enabled_chk_->setChecked(cfg.get_webhook_enabled());
    webhook_enabled_chk_->setStyleSheet("color: #58a6ff; font-weight: bold; font-size: 13px;");
    webhook_row1->addWidget(webhook_enabled_chk_);

    webhook_row1->addSpacing(20);
    webhook_row1->addWidget(new QLabel("请求超时:", this));
    webhook_timeout_spin_ = new QSpinBox(this);
    webhook_timeout_spin_->setRange(500, 15000);
    webhook_timeout_spin_->setSingleStep(500);
    webhook_timeout_spin_->setValue(cfg.get_webhook_timeout_ms());
    webhook_timeout_spin_->setSuffix(" ms");
    webhook_row1->addWidget(webhook_timeout_spin_);
    webhook_row1->addStretch();
    webhook_layout->addLayout(webhook_row1);

    auto* webhook_row2 = new QHBoxLayout();
    webhook_row2->addWidget(new QLabel("推送目标 URL:", this));
    webhook_url_edit_ = new QLineEdit(cfg.get_webhook_url(), this);
    webhook_url_edit_->setPlaceholderText("例如: http://127.0.0.1:8088/api/v1/events");
    btn_test_ping_ = new QPushButton("⚡  测试推送 Ping", this);
    btn_test_ping_->setStyleSheet(
        "QPushButton { background: #1f6feb; color: #ffffff; font-weight: bold; padding: 6px 14px; border-radius: 4px; border: 1px solid #388bfd; }"
        "QPushButton:hover { background: #388bfd; }"
        "QPushButton:disabled { background: #30363d; color: #8b949e; border-color: #30363d; }"
    );
    webhook_row2->addWidget(webhook_url_edit_, 1);
    webhook_row2->addWidget(btn_test_ping_);
    webhook_layout->addLayout(webhook_row2);

    auto* webhook_row3 = new QHBoxLayout();
    webhook_row3->addWidget(new QLabel("认证 Token (Bearer):", this));
    webhook_token_edit_ = new QLineEdit(cfg.get_webhook_token(), this);
    webhook_token_edit_->setPlaceholderText("可选，如填写将添加 Authorization: Bearer <token> 请求头");
    webhook_row3->addWidget(webhook_token_edit_, 1);
    webhook_layout->addLayout(webhook_row3);

    webhook_status_label_ = new QLabel("就绪 - 点击「测试推送 Ping」可检验远端接收服务器状态", this);
    webhook_status_label_->setStyleSheet("color: #8b949e; font-size: 12px; margin-top: 2px; padding: 4px 8px; background: #0d1117; border-radius: 4px; border: 1px solid #21262d;");
    webhook_layout->addWidget(webhook_status_label_);

    auto* webhook_hint = new QLabel(
        "• 传输协议: 标准 HTTP POST (application/json)，网络推送在独立事件循环中异步触发，完全非阻塞；\n"
        "• 推送内容: 包含完整进出事实 (PASSAGE / ENTRY / EXIT / 置信度 / 切片路径) 及超载告警 (CROWD_SURGE)；\n"
        "• 容灾机制: 若云端接口不可用或超时，将在日志记录警告，绝不影响本地视频 30fps 实时推理帧率。",
        this
    );
    webhook_hint->setStyleSheet("color: #8b949e; font-size: 12px; margin-top: 2px;");
    webhook_layout->addWidget(webhook_hint);

    main_layout->addWidget(webhook_group);

    // 5. AI 硬件信息
    auto* hw_group = new QGroupBox("AI 推理硬件环境", this);
    auto* hw_layout = new QVBoxLayout(hw_group);

    hardware_info_label_ = new QLabel(
        "• 推理框架: ONNX Runtime C++ API (v1.24)\n"
        "• 目标平台: Windows 10/11 x64\n"
        "• 检测模型: YOLOv8n (640x640 FP32, Person Class Only)\n"
        "• 多目标跟踪: ByteTrack C++ (Kalman Filter + IoU)\n"
        "• 数据库模式: SQLite 3 (WAL 并发模式已激活)\n"
        "• 网络总线: Qt6 Network Webhook (异步 HTTP POST 已集成)\n"
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

    connect(btn_test_ping_, &QPushButton::clicked, this, [this]() {
        QString url = webhook_url_edit_->text().trimmed();
        QString token = webhook_token_edit_->text().trimmed();
        int timeout = webhook_timeout_spin_->value();

        btn_test_ping_->setEnabled(false);
        btn_test_ping_->setText("正在探测...");
        webhook_status_label_->setText("正在向目标服务器发送测试探测包 (PING)...");
        webhook_status_label_->setStyleSheet("color: #58a6ff; font-size: 12px; padding: 4px 8px; background: #0d1117; border-radius: 4px; border: 1px solid #1f6feb;");

        WebhookPusher::instance().test_ping(url, token, timeout, [this](bool success, int code, const QString& msg, qint64 latency) {
            btn_test_ping_->setEnabled(true);
            btn_test_ping_->setText("⚡  测试推送 Ping");
            if (success) {
                webhook_status_label_->setText(QString("✅ 云端测试探测成功！响应码: HTTP %1, 往返耗时: %2 ms").arg(code).arg(latency));
                webhook_status_label_->setStyleSheet("color: #3fb950; font-size: 12px; font-weight: bold; padding: 4px 8px; background: #0d1117; border-radius: 4px; border: 1px solid #238636;");
            } else {
                webhook_status_label_->setText(QString("❌ 探测失败: %1 (耗时: %2 ms)").arg(msg).arg(latency));
                webhook_status_label_->setStyleSheet("color: #f85149; font-size: 12px; font-weight: bold; padding: 4px 8px; background: #0d1117; border-radius: 4px; border: 1px solid #da3633;");
            }
        });
    });
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

    cfg.set_webhook_enabled(webhook_enabled_chk_->isChecked());
    cfg.set_webhook_url(webhook_url_edit_->text());
    cfg.set_webhook_token(webhook_token_edit_->text());
    cfg.set_webhook_timeout_ms(webhook_timeout_spin_->value());

    cfg.save();

    emit video_source_changed(video_input_edit_->text());
    QMessageBox::information(this, "成功", "设置已保存至 configs/flowsight.ini！已实时应用并持久化保存。");
}
