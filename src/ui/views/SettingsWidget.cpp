#include "SettingsWidget.hpp"
#include "storage/ConfigManager.hpp"
#include "storage/DatabaseManager.hpp"
#include "storage/StoragePruner.hpp"
#include "network/WebhookPusher.hpp"
#include "network/EdgeReportManager.hpp"
#include <QJsonArray>
#include <QJsonObject>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollArea>

SettingsWidget::SettingsWidget(QWidget* parent) : QWidget(parent) {
    setup_ui();
}

void SettingsWidget::setup_ui() {
    auto* root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->setSpacing(0);

    auto* scroll_area = new QScrollArea(this);
    scroll_area->setWidgetResizable(true);
    scroll_area->setFrameShape(QFrame::NoFrame);
    scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_area->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto* container = new QWidget(scroll_area);
    container->setObjectName("settings_container");
    auto* main_layout = new QVBoxLayout(container);
    main_layout->setContentsMargins(16, 16, 16, 16);
    main_layout->setSpacing(14);

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

    // 5. 云端协同与中台设备绑定 (Cloud SaaS & Device Binding)
    auto* cloud_group = new QGroupBox("云端协同与中台设备绑定 (Cloud SaaS & Device Binding)", this);
    cloud_group->setStyleSheet(
        "QGroupBox { font-weight: bold; border: 1px solid #1f6feb; border-radius: 4px; margin-top: 10px; padding-top: 10px; } "
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 10px; color: #58a6ff; }"
    );
    auto* cloud_layout = new QVBoxLayout(cloud_group);
    cloud_layout->setSpacing(10);

    // 行 1: 云端服务地址与客户端节点名称
    auto* cloud_row1 = new QHBoxLayout();
    cloud_row1->addWidget(new QLabel("云端中台 URL:", this));
    cloud_url_edit_ = new QLineEdit(cfg.get_cloud_server_url(), this);
    cloud_url_edit_->setPlaceholderText("例如: http://localhost:8080");
    cloud_row1->addWidget(cloud_url_edit_, 1);

    cloud_row1->addSpacing(16);
    cloud_row1->addWidget(new QLabel("本节点名称:", this));
    cloud_client_name_edit_ = new QLineEdit(cfg.get_cloud_client_name(), this);
    cloud_client_name_edit_->setPlaceholderText("例如: FlowSight-Node-1");
    cloud_row1->addWidget(cloud_client_name_edit_, 1);
    cloud_layout->addLayout(cloud_row1);

    // 行 2: 手机号与密码登录
    auto* cloud_row2 = new QHBoxLayout();
    cloud_row2->addWidget(new QLabel("管理手机号:", this));
    cloud_phone_edit_ = new QLineEdit(cfg.get_cloud_user_phone(), this);
    cloud_phone_edit_->setPlaceholderText("手机号 (如 13800000000)");
    cloud_row2->addWidget(cloud_phone_edit_, 1);

    cloud_row2->addSpacing(16);
    cloud_row2->addWidget(new QLabel("登录密码:", this));
    cloud_password_edit_ = new QLineEdit(this);
    cloud_password_edit_->setEchoMode(QLineEdit::Password);
    cloud_password_edit_->setPlaceholderText("默认与手机号同号或管理密码");
    cloud_password_edit_->setText(cfg.get_cloud_user_phone());
    cloud_row2->addWidget(cloud_password_edit_, 1);

    btn_cloud_login_ = new QPushButton("🔐 登录并拉取点位", this);
    btn_cloud_login_->setStyleSheet(
        "QPushButton { background: #238636; color: #ffffff; font-weight: bold; padding: 6px 14px; border-radius: 4px; border: 1px solid #2ea043; }"
        "QPushButton:hover { background: #2ea043; }"
    );
    cloud_row2->addWidget(btn_cloud_login_);
    cloud_layout->addLayout(cloud_row2);

    // 行 3: 监控点位选择与绑定确认
    auto* cloud_row3 = new QHBoxLayout();
    cloud_row3->addWidget(new QLabel("所属监控点位:", this));
    cloud_point_combo_ = new QComboBox(this);
    cloud_point_combo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // 预填当前已绑定的点位
    if (cfg.get_cloud_point_id() > 0) {
        QVariantMap pdata;
        pdata["id"] = cfg.get_cloud_point_id();
        pdata["name"] = cfg.get_cloud_point_name();
        pdata["token"] = cfg.get_cloud_point_token();
        cloud_point_combo_->addItem(QString("[%1] %2 (已绑定)").arg(cfg.get_cloud_point_id()).arg(cfg.get_cloud_point_name()), pdata);
    }
    cloud_row3->addWidget(cloud_point_combo_, 1);

    btn_bind_point_ = new QPushButton("🔗 确认绑定此点位", this);
    btn_bind_point_->setStyleSheet(
        "QPushButton { background: #1f6feb; color: #ffffff; font-weight: bold; padding: 6px 14px; border-radius: 4px; border: 1px solid #388bfd; }"
        "QPushButton:hover { background: #388bfd; }"
    );
    cloud_row3->addWidget(btn_bind_point_);

    btn_sync_config_ = new QPushButton("🔄 同步云端配置 (RTSP流)", this);
    btn_sync_config_->setStyleSheet(
        "QPushButton { background: #238636; color: #ffffff; font-weight: bold; padding: 6px 14px; border-radius: 4px; border: 1px solid #2ea043; }"
        "QPushButton:hover { background: #2ea043; }"
    );
    cloud_row3->addWidget(btn_sync_config_);

    btn_test_report_ = new QPushButton("⚡ 立即上报测试包", this);
    btn_test_report_->setStyleSheet(
        "QPushButton { background: #8957e5; color: #ffffff; font-weight: bold; padding: 6px 14px; border-radius: 4px; border: 1px solid #a371f7; }"
        "QPushButton:hover { background: #a371f7; }"
    );
    cloud_row3->addWidget(btn_test_report_);
    cloud_layout->addLayout(cloud_row3);

    // 状态提示条
    cloud_status_info_label_ = new QLabel(this);
    cloud_status_info_label_->setStyleSheet("color: #8b949e; font-size: 12px; padding: 6px 10px; background: #0d1117; border-radius: 4px; border: 1px solid #21262d;");
    if (cfg.get_cloud_point_id() > 0 && !cfg.get_cloud_point_token().isEmpty()) {
        cloud_status_info_label_->setText(QString("🟢 当前绑定: %1 (ID: %2) | 节点名: %3 | Token: %4...")
            .arg(cfg.get_cloud_point_name())
            .arg(cfg.get_cloud_point_id())
            .arg(cfg.get_cloud_client_name())
            .arg(cfg.get_cloud_point_token().left(16)));
        cloud_status_info_label_->setStyleSheet("color: #3fb950; font-size: 12px; font-weight: 500; padding: 6px 10px; background: #0d1117; border-radius: 4px; border: 1px solid #238636;");
    } else {
        cloud_status_info_label_->setText("⚪ 尚未绑定云端点位 - 请输入管理员手机号和密码后点击「登录并拉取点位」");
    }
    cloud_layout->addWidget(cloud_status_info_label_);

    auto* cloud_hint = new QLabel(
        "• 业务闭环: FlowSight 边端客户端通过手机号登录云端 SaaS，选择对应监控通道点位即可完成一键绑定；\n"
        "• 智能节流: 客流识别数据将按 10 秒时间窗口聚合并经 GZIP 高效压缩后异步上报，断网自动转存本地离线队列；\n"
        "• 实时联动: 边端绑定的点位数据将实时推送到手机端 H5 决策看板 (包含日/周/月/季度多维报表)。",
        this
    );
    cloud_hint->setStyleSheet("color: #8b949e; font-size: 12px; margin-top: 2px;");
    cloud_layout->addWidget(cloud_hint);

    main_layout->addWidget(cloud_group);

    // 6. AI 硬件信息
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

    // 6. 数据清理与系统维护 (Data Maintenance)
    auto* maint_group = new QGroupBox("数据清理与系统维护 (Data Maintenance)", this);
    maint_group->setStyleSheet(
        "QGroupBox { font-weight: bold; border: 1px solid #4a1e1e; border-radius: 4px; margin-top: 10px; padding-top: 10px; } "
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 10px; color: #f85149; }"
    );
    auto* maint_layout = new QVBoxLayout(maint_group);
    maint_layout->setSpacing(8);

    auto* maint_hint = new QLabel(
        "• 数据库记录: 彻底清空通行历史事件 (`passage_events`)、小时统计聚合表 (`stats_hourly`) 并收缩释放 SQLite 磁盘碎片；\n"
        "• 磁盘录像切片: 物理删除 `clips/` 目录下所有已保存的视频切片文件，彻底释放磁盘占用；\n"
        "• 内存与界面归零: 实时监控看板今日进、出、总客流及实时事件流水即刻归零，所有图表与证据回溯列表同步清空；\n"
        "• ⚠️ 警告: 该操作为永久物理删除，数据清除后将无法恢复，请谨慎操作！",
        this
    );
    maint_hint->setStyleSheet("color: #f0883e; font-size: 12px; line-height: 150%;");
    maint_layout->addWidget(maint_hint);

    auto* maint_action_row = new QHBoxLayout();
    reset_rois_chk_ = new QCheckBox("同时重置所有防区标定线与进出反向配置 (Reset Zone & Direction Configs)", this);
    reset_rois_chk_->setStyleSheet("color: #8b949e; font-size: 12px;");
    maint_action_row->addWidget(reset_rois_chk_);
    maint_action_row->addStretch();

    btn_clear_data_ = new QPushButton("⚠️ 彻底清空所有历史数据与录像切片", this);
    btn_clear_data_->setStyleSheet(
        "QPushButton { background: #8c1d18; color: #ffffff; font-weight: bold; font-size: 12px; padding: 7px 16px; border-radius: 4px; border: 1px solid #da3633; }"
        "QPushButton:hover { background: #b3231c; border-color: #f85149; }"
        "QPushButton:pressed { background: #6e1410; }"
    );
    maint_action_row->addWidget(btn_clear_data_);
    maint_layout->addLayout(maint_action_row);

    main_layout->addWidget(maint_group);

    main_layout->addSpacing(8);

    btn_save_ = new QPushButton("保存并应用设置", container);
    btn_save_->setStyleSheet("padding: 8px 20px; background: #0984e3; color: white; font-weight: bold; border-radius: 4px;");
    main_layout->addWidget(btn_save_, 0, Qt::AlignRight);

    scroll_area->setWidget(container);
    root_layout->addWidget(scroll_area);

    connect(btn_browse_, &QPushButton::clicked, this, &SettingsWidget::on_btn_browse);
    connect(btn_save_, &QPushButton::clicked, this, &SettingsWidget::on_btn_save);
    connect(btn_clear_data_, &QPushButton::clicked, this, &SettingsWidget::on_btn_clear_data);

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

    connect(btn_cloud_login_, &QPushButton::clicked, this, [this]() {
        QString url = cloud_url_edit_->text().trimmed();
        QString phone = cloud_phone_edit_->text().trimmed();
        QString pwd = cloud_password_edit_->text().trimmed();

        if (url.isEmpty() || phone.isEmpty() || pwd.isEmpty()) {
            QMessageBox::warning(this, "提示", "请填写完整的云端中台 URL、手机号及登录密码！");
            return;
        }

        btn_cloud_login_->setEnabled(false);
        btn_cloud_login_->setText("正在验证...");
        cloud_status_info_label_->setText("正在向云端认证账号凭证并拉取资产点位...");
        cloud_status_info_label_->setStyleSheet("color: #58a6ff; font-size: 12px; padding: 6px 10px; background: #0d1117; border-radius: 4px; border: 1px solid #1f6feb;");

        EdgeReportManager::instance().login_with_credentials(url, phone, pwd, [this, phone](bool success, const QString& msg, const QJsonArray& points) {
            btn_cloud_login_->setEnabled(true);
            btn_cloud_login_->setText("🔐 登录并拉取点位");

            if (!success) {
                cloud_status_info_label_->setText(QString("❌ 登录失败: %1").arg(msg));
                cloud_status_info_label_->setStyleSheet("color: #f85149; font-size: 12px; padding: 6px 10px; background: #0d1117; border-radius: 4px; border: 1px solid #da3633;");
                return;
            }

            cloud_point_combo_->clear();
            for (const auto& val : points) {
                QJsonObject p = val.toObject();
                qint64 pid = p["id"].toInteger();
                QString pname = p["pointName"].toString();
                QString pcode = p["pointCode"].toString();
                QString ptoken = p["pointToken"].toString();

                QVariantMap map;
                map["id"] = pid;
                map["name"] = pname;
                map["token"] = ptoken;

                cloud_point_combo_->addItem(QString("[%1] %2 (%3)").arg(pid).arg(pname).arg(pcode), map);
            }

            cloud_status_info_label_->setText(QString("✅ 登录成功！手机号 %1 获取到 %2 个监控点位，请选择并绑定。").arg(phone).arg(points.size()));
            cloud_status_info_label_->setStyleSheet("color: #3fb950; font-size: 12px; font-weight: bold; padding: 6px 10px; background: #0d1117; border-radius: 4px; border: 1px solid #238636;");
        });
    });

    connect(btn_bind_point_, &QPushButton::clicked, this, [this]() {
        if (cloud_point_combo_->count() == 0) {
            QMessageBox::warning(this, "提示", "请先登录云端拉取点位列表！");
            return;
        }

        QVariantMap map = cloud_point_combo_->currentData().toMap();
        qint64 pid = map["id"].toLongLong();
        QString pname = map["name"].toString();
        QString ptoken = map["token"].toString();
        QString url = cloud_url_edit_->text().trimmed();
        QString client_name = cloud_client_name_edit_->text().trimmed();

        if (client_name.isEmpty()) client_name = "FlowSight-Node-1";

        EdgeReportManager::instance().bind_point(url, pid, pname, ptoken, client_name);

        cloud_status_info_label_->setText(QString("🟢 绑定成功！已接入云端点位: %1 (ID: %2) | 节点: %3").arg(pname).arg(pid).arg(client_name));
        cloud_status_info_label_->setStyleSheet("color: #3fb950; font-size: 12px; font-weight: bold; padding: 6px 10px; background: #0d1117; border-radius: 4px; border: 1px solid #238636;");
        QMessageBox::information(this, "绑定成功", QString("客户端已成功与云端点位「%1」建立关联！\n系统将自动同步拉取摄像头视频流配置。").arg(pname));

        // 绑定成功后自动触发拉取云端 RTSP 流与点位详细参数
        btn_sync_config_->click();
    });

    connect(btn_sync_config_, &QPushButton::clicked, this, [this]() {
        btn_sync_config_->setEnabled(false);
        btn_sync_config_->setText("正在同步...");
        cloud_status_info_label_->setText("正在从云端拉取点位参数与摄像头 RTSP 配置...");

        EdgeReportManager::instance().fetch_cloud_config([this](bool success, const QString& msg, const QJsonObject& cfg_data) {
            btn_sync_config_->setEnabled(true);
            btn_sync_config_->setText("🔄 同步云端配置 (RTSP流)");

            if (success) {
                QString pname = cfg_data.value("pointName").toString();
                QString rtsp = cfg_data.value("cameraRtsp").toString();
                QString res = cfg_data.value("resolution").toString();

                auto& cfg = ConfigManager::instance();
                if (!pname.isEmpty()) {
                    cfg.set_cloud_point_name(pname);
                }

                if (!rtsp.trimmed().isEmpty()) {
                    video_input_edit_->setText(rtsp.trimmed());
                    cfg.set_video_source(rtsp.trimmed());
                    cfg.save();
                    emit video_source_changed(rtsp.trimmed());
                    cloud_status_info_label_->setText(QString("✅ 云端配置已同步！点位: %1 | 摄像头 RTSP: %2 | 已自适应热重载视频流").arg(pname).arg(rtsp));
                    cloud_status_info_label_->setStyleSheet("color: #3fb950; font-size: 12px; font-weight: bold; padding: 6px 10px; background: #0d1117; border-radius: 4px; border: 1px solid #238636;");
                    QMessageBox::information(this, "云端配置已同步", QString("成功从云端中台拉取最新点位资产！\n点位名称: %1\n摄像头流: %2\n分辨率: %3\n\n已自适应热重载视频流！").arg(pname).arg(rtsp).arg(res));
                } else {
                    cfg.save();
                    cloud_status_info_label_->setText(QString("✅ 云端配置已同步！点位: %1 (云端未配置 RTSP 流地址)").arg(pname));
                    cloud_status_info_label_->setStyleSheet("color: #3fb950; font-size: 12px; font-weight: bold; padding: 6px 10px; background: #0d1117; border-radius: 4px; border: 1px solid #238636;");
                    QMessageBox::information(this, "云端配置已同步", QString("点位名称: %1\n云端尚未录入 RTSP 视频流地址，保持当前视频输入源不变。").arg(pname));
                }
            } else {
                cloud_status_info_label_->setText(QString("❌ 同步云端配置失败: %1").arg(msg));
                cloud_status_info_label_->setStyleSheet("color: #f85149; font-size: 12px; font-weight: bold; padding: 6px 10px; background: #0d1117; border-radius: 4px; border: 1px solid #da3633;");
                QMessageBox::warning(this, "同步失败", msg);
            }
        });
    });

    connect(btn_test_report_, &QPushButton::clicked, this, [this]() {
        btn_test_report_->setEnabled(false);
        btn_test_report_->setText("正在上传...");
        cloud_status_info_label_->setText("正在向云端提交测试指标聚合包 (GZIP 压缩)...");

        EdgeReportManager::instance().send_test_batch([this](bool success, const QString& msg, qint64 latency) {
            btn_test_report_->setEnabled(true);
            btn_test_report_->setText("⚡ 立即上报测试包");
            if (success) {
                cloud_status_info_label_->setText(QString("✅ 测试聚合包上报成功！%1 (耗时: %2 ms)").arg(msg).arg(latency));
                cloud_status_info_label_->setStyleSheet("color: #3fb950; font-size: 12px; font-weight: bold; padding: 6px 10px; background: #0d1117; border-radius: 4px; border: 1px solid #238636;");
            } else {
                cloud_status_info_label_->setText(QString("❌ 上报测试包失败: %1 (耗时: %2 ms)").arg(msg).arg(latency));
                cloud_status_info_label_->setStyleSheet("color: #f85149; font-size: 12px; font-weight: bold; padding: 6px 10px; background: #0d1117; border-radius: 4px; border: 1px solid #da3633;");
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

    cfg.set_cloud_server_url(cloud_url_edit_->text().trimmed());
    cfg.set_cloud_client_name(cloud_client_name_edit_->text().trimmed());
    cfg.set_cloud_user_phone(cloud_phone_edit_->text().trimmed());

    cfg.save();

    emit video_source_changed(video_input_edit_->text());
    emit settings_applied(surge_threshold_spin_->value(), surge_enabled_chk_->isChecked(), surge_beep_chk_->isChecked());
    QMessageBox::information(this, "成功", "设置已保存至 configs/flowsight.ini！已实时应用并持久化保存。");
}

void SettingsWidget::on_btn_clear_data() {
    auto reply = QMessageBox::warning(
        this,
        "高危数据清除确认",
        "确定要清空全部通行历史记录及本地视频切片吗？\n\n"
        "1. 数据库中的所有通行事实 (`passage_events`) 及小时统计聚合将被物理抹除；\n"
        "2. clips 录像切片目录下的所有切片视频文件将被彻底删除；\n"
        "3. 实时监控看板今日进、出、总人次流水将即刻归零。\n\n"
        "⚠️ 本操作不可逆！是否确认继续？",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (reply != QMessageBox::Yes) {
        return;
    }

    bool reset_rois = (reset_rois_chk_ && reset_rois_chk_->isChecked());

    // 1. 物理清空 clips 目录下所有切片视频
    qint64 freed_bytes = 0;
    int deleted_files = StoragePruner::clear_all_clips("clips", &freed_bytes);

    // 2. 清空数据库记录 (passage_events, stats_hourly, VACUUM)
    int deleted_events = 0;
    bool db_ok = DatabaseManager::instance().clear_all_history_data(reset_rois, &deleted_events);

    // 3. 广播信号给 MainWindow 联动清空内存与界面
    emit data_cleared(reset_rois);

    // 4. 统计并弹窗展示清理成果
    double freed_mb = freed_bytes / (1024.0 * 1024.0);
    QString summary_msg = QString(
        "数据清理已彻底完成！\n\n"
        "• 数据库清除: 成功移除 %1 条通行历史与统计记录\n"
        "• 磁盘切片清理: 成功删除 %2 个视频切片文件，释放磁盘空间 %3 MB\n"
        "• 内存与界面: 今日进出客流与流水记录已全部归零"
    ).arg(deleted_events).arg(deleted_files).arg(QString::number(freed_mb, 'f', 2));

    if (reset_rois) {
        summary_msg += "\n• 防区配置: 通道标定线与进出反向配置已恢复初始预设";
    }

    QMessageBox::information(this, "系统清理完成", summary_msg);
}
