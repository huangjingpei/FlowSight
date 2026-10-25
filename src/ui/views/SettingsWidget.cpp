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
    main_layout->setContentsMargins(20, 16, 20, 16);
    main_layout->setSpacing(14);

    auto& cfg = ConfigManager::instance();

    // ==========================================
    // 1. 视频输入源设置 (卡片 1)
    // ==========================================
    auto* src_group = new QGroupBox("视频输入源", this);
    auto* src_layout = new QHBoxLayout(src_group);
    src_layout->setContentsMargins(14, 14, 14, 14);
    src_layout->setSpacing(10);

    src_layout->addWidget(new QLabel("视频路径 / RTSP 流地址:", this));
    video_input_edit_ = new QLineEdit(cfg.get_video_source(), this);
    video_input_edit_->setPlaceholderText("本地 MP4 文件路径 或 rtsp:// 工业网络摄像机视频流");
    src_layout->addWidget(video_input_edit_, 1);

    btn_browse_ = new QPushButton("📁 浏览文件...", this);
    btn_browse_->setStyleSheet("QPushButton { padding: 5px 14px; font-weight: 500; }");
    src_layout->addWidget(btn_browse_);
    main_layout->addWidget(src_group);

    // ==========================================
    // 2. 云端平台与点位管理 (卡片 2 - 扁平直观，纯商户资产对齐)
    // ==========================================
    auto* cloud_group = new QGroupBox("云端平台与点位管理", this);
    auto* cloud_layout = new QVBoxLayout(cloud_group);
    cloud_layout->setContentsMargins(14, 14, 14, 14);
    cloud_layout->setSpacing(10);

    // 行 1: 服务地址与节点标识
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

    // 行 2: 商户账号登录与点位拉取
    auto* cloud_row2 = new QHBoxLayout();
    cloud_row2->addWidget(new QLabel("商户登录手机号:", this));
    cloud_phone_edit_ = new QLineEdit(cfg.get_cloud_user_phone(), this);
    cloud_phone_edit_->setPlaceholderText("例如: 13454118762");
    cloud_row2->addWidget(cloud_phone_edit_, 1);

    cloud_row2->addSpacing(16);
    cloud_row2->addWidget(new QLabel("登录密码:", this));
    cloud_password_edit_ = new QLineEdit(this);
    cloud_password_edit_->setEchoMode(QLineEdit::Password);
    cloud_password_edit_->setPlaceholderText("商户登录密码");
    cloud_password_edit_->setText(cfg.get_cloud_user_phone());
    cloud_row2->addWidget(cloud_password_edit_, 1);

    btn_cloud_login_ = new QPushButton("🔐 登录并拉取点位", this);
    btn_cloud_login_->setStyleSheet("QPushButton { background: #1f6feb; color: #ffffff; font-weight: bold; padding: 6px 14px; border-radius: 4px; }");
    cloud_row2->addWidget(btn_cloud_login_);
    cloud_layout->addLayout(cloud_row2);

    // 行 3: 关联业务点位选择与双向同步
    auto* cloud_row3 = new QHBoxLayout();
    cloud_row3->addWidget(new QLabel("选择关联点位:", this));
    cloud_point_combo_ = new QComboBox(this);
    cloud_point_combo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    if (cfg.get_cloud_point_id() > 0) {
        QVariantMap pdata;
        pdata["id"] = cfg.get_cloud_point_id();
        pdata["name"] = cfg.get_cloud_point_name();
        pdata["token"] = cfg.get_cloud_point_token();
        cloud_point_combo_->addItem(QString("[%1] %2").arg(cfg.get_cloud_point_id()).arg(cfg.get_cloud_point_name()), pdata);
    }
    cloud_row3->addWidget(cloud_point_combo_, 1);

    btn_bind_point_ = new QPushButton("🔗 确认绑定", this);
    btn_bind_point_->setStyleSheet("QPushButton { background: #238636; color: #ffffff; font-weight: bold; padding: 6px 14px; border-radius: 4px; }");
    cloud_row3->addWidget(btn_bind_point_);

    btn_sync_config_ = new QPushButton("🔄 同步云端配置 (RTSP流)", this);
    btn_sync_config_->setStyleSheet("QPushButton { background: #2ea043; color: #ffffff; font-weight: bold; padding: 6px 14px; border-radius: 4px; }");
    cloud_row3->addWidget(btn_sync_config_);

    btn_test_report_ = new QPushButton("⚡ 上报测试包", this);
    btn_test_report_->setStyleSheet("QPushButton { background: #8957e5; color: #ffffff; font-weight: bold; padding: 6px 14px; border-radius: 4px; }");
    cloud_row3->addWidget(btn_test_report_);
    cloud_layout->addLayout(cloud_row3);

    // 备用隐藏参数指针 (保持向下兼容)
    cloud_point_code_edit_ = new QLineEdit(cfg.get_cloud_point_code(), this);
    cloud_point_code_edit_->setVisible(false);
    cloud_device_pwd_edit_ = new QLineEdit(cfg.get_cloud_device_password(), this);
    cloud_device_pwd_edit_->setVisible(false);
    btn_device_auth_ = new QPushButton(this);
    btn_device_auth_->setVisible(false);
    cloud_pairing_code_edit_ = new QLineEdit(this);
    cloud_pairing_code_edit_->setVisible(false);
    btn_pair_with_code_ = new QPushButton(this);
    btn_pair_with_code_->setVisible(false);

    // 行 4: 极简状态胶囊徽章
    cloud_status_info_label_ = new QLabel(this);
    auto update_status_badge = [this]() {
        auto& cfg = ConfigManager::instance();
        if (cfg.get_cloud_point_id() > 0 && !cfg.get_cloud_point_token().isEmpty()) {
            QString company = cfg.get_cloud_company_name();
            QString phone = cfg.get_cloud_user_phone();
            cloud_status_info_label_->setText(QString("🟢 已绑定点位: [%1] %2  │  商户: %3 (%4)  │  状态: 在线")
                .arg(cfg.get_cloud_point_id())
                .arg(cfg.get_cloud_point_name())
                .arg(company.isEmpty() ? "万科物业" : company)
                .arg(phone.isEmpty() ? "13454118762" : phone));
            cloud_status_info_label_->setStyleSheet(
                "color: #2ea043; font-weight: 600; font-size: 12px; padding: 7px 12px; background: rgba(46, 160, 67, 0.12); border-radius: 4px; border: 1px solid rgba(46, 160, 67, 0.4);"
            );
        } else {
            cloud_status_info_label_->setText("⚪ 未关联云端点位 - 请输入商户手机号与密码登录后选择点位绑定");
            cloud_status_info_label_->setStyleSheet(
                "color: #8b949e; font-size: 12px; padding: 7px 12px; background: rgba(139, 148, 158, 0.1); border-radius: 4px; border: 1px solid rgba(139, 148, 158, 0.3);"
            );
        }
    };
    update_status_badge();
    cloud_layout->addWidget(cloud_status_info_label_);

    main_layout->addWidget(cloud_group);

    // ==========================================
    // 3. 告警风控与网络推送 (卡片 3)
    // ==========================================
    auto* alert_group = new QGroupBox("告警风控与网络推送", this);
    auto* alert_layout = new QVBoxLayout(alert_group);
    alert_layout->setContentsMargins(14, 14, 14, 14);
    alert_layout->setSpacing(10);

    // 行 1: 客流超载告警设置
    auto* alert_row1 = new QHBoxLayout();
    surge_enabled_chk_ = new QCheckBox("启用区域人员超载告警", this);
    surge_enabled_chk_->setChecked(cfg.get_crowd_surge_enabled());
    surge_enabled_chk_->setStyleSheet("color: #f85149; font-weight: bold;");
    alert_row1->addWidget(surge_enabled_chk_);

    alert_row1->addSpacing(20);
    alert_row1->addWidget(new QLabel("超载人数报警阈值:", this));
    surge_threshold_spin_ = new QSpinBox(this);
    surge_threshold_spin_->setRange(1, 200);
    surge_threshold_spin_->setValue(cfg.get_crowd_surge_threshold());
    surge_threshold_spin_->setSuffix(" 人");
    alert_row1->addWidget(surge_threshold_spin_);

    alert_row1->addSpacing(20);
    surge_beep_chk_ = new QCheckBox("声音蜂鸣提示", this);
    surge_beep_chk_->setChecked(cfg.get_crowd_surge_beep());
    alert_row1->addWidget(surge_beep_chk_);
    alert_row1->addStretch();
    alert_layout->addLayout(alert_row1);

    // 行 2: Webhook 事件推送总线
    auto* alert_row2 = new QHBoxLayout();
    webhook_enabled_chk_ = new QCheckBox("启用 Webhook 实时推送", this);
    webhook_enabled_chk_->setChecked(cfg.get_webhook_enabled());
    webhook_enabled_chk_->setStyleSheet("color: #58a6ff; font-weight: 500;");
    alert_row2->addWidget(webhook_enabled_chk_);

    alert_row2->addSpacing(14);
    alert_row2->addWidget(new QLabel("推送地址:", this));
    webhook_url_edit_ = new QLineEdit(cfg.get_webhook_url(), this);
    webhook_url_edit_->setPlaceholderText("http://127.0.0.1:8088/api/v1/events");
    alert_row2->addWidget(webhook_url_edit_, 1);

    alert_row2->addSpacing(10);
    alert_row2->addWidget(new QLabel("Token:", this));
    webhook_token_edit_ = new QLineEdit(cfg.get_webhook_token(), this);
    webhook_token_edit_->setPlaceholderText("可选 Bearer Token");
    alert_row2->addWidget(webhook_token_edit_, 1);

    alert_row2->addSpacing(10);
    alert_row2->addWidget(new QLabel("超时:", this));
    webhook_timeout_spin_ = new QSpinBox(this);
    webhook_timeout_spin_->setRange(500, 15000);
    webhook_timeout_spin_->setSingleStep(500);
    webhook_timeout_spin_->setValue(cfg.get_webhook_timeout_ms());
    webhook_timeout_spin_->setSuffix(" ms");
    alert_row2->addWidget(webhook_timeout_spin_);

    btn_test_ping_ = new QPushButton("⚡ 测试 Ping", this);
    btn_test_ping_->setStyleSheet("QPushButton { padding: 5px 12px; font-weight: 500; }");
    alert_row2->addWidget(btn_test_ping_);
    alert_layout->addLayout(alert_row2);

    webhook_status_label_ = new QLabel("就绪 - 点击「测试 Ping」可检验远端服务器网络连通性", this);
    webhook_status_label_->setStyleSheet("color: #8b949e; font-size: 11px; padding: 4px 8px; background: rgba(139, 148, 158, 0.08); border-radius: 3px;");
    alert_layout->addWidget(webhook_status_label_);

    main_layout->addWidget(alert_group);

    // ==========================================
    // 4. 存储策略与数据维护 (卡片 4)
    // ==========================================
    auto* maint_group = new QGroupBox("存储策略与数据维护", this);
    auto* maint_layout = new QHBoxLayout(maint_group);
    maint_layout->setContentsMargins(14, 14, 14, 14);
    maint_layout->setSpacing(12);

    maint_layout->addWidget(new QLabel("录像切片保留天数:", this));
    retention_spin_ = new QSpinBox(this);
    retention_spin_->setRange(1, 365);
    retention_spin_->setValue(cfg.get_clip_retention_days());
    maint_layout->addWidget(retention_spin_);
    maint_layout->addWidget(new QLabel("天 (超期切片自动物理清理)", this));

    maint_layout->addStretch();

    reset_rois_chk_ = new QCheckBox("同时重置防区通道标定线", this);
    reset_rois_chk_->setStyleSheet("color: #8b949e; font-size: 12px;");
    maint_layout->addWidget(reset_rois_chk_);

    btn_clear_data_ = new QPushButton("⚠️ 清空历史数据与录像切片", this);
    btn_clear_data_->setStyleSheet(
        "QPushButton { background: #8c1d18; color: #ffffff; font-weight: bold; font-size: 12px; padding: 6px 14px; border-radius: 4px; border: 1px solid #da3633; }"
        "QPushButton:hover { background: #b3231c; border-color: #f85149; }"
        "QPushButton:pressed { background: #6e1410; }"
    );
    maint_layout->addWidget(btn_clear_data_);

    main_layout->addWidget(maint_group);

    // ==========================================
    // 底部常驻保存按钮
    // ==========================================
    main_layout->addSpacing(4);
    btn_save_ = new QPushButton("💾 保存并应用设置", container);
    btn_save_->setStyleSheet("padding: 8px 24px; background: #0984e3; color: white; font-weight: bold; font-size: 13px; border-radius: 4px;");
    main_layout->addWidget(btn_save_, 0, Qt::AlignRight);

    scroll_area->setWidget(container);
    root_layout->addWidget(scroll_area);

    // ==========================================
    // 信号槽逻辑绑定
    // ==========================================
    connect(btn_browse_, &QPushButton::clicked, this, &SettingsWidget::on_btn_browse);
    connect(btn_save_, &QPushButton::clicked, this, &SettingsWidget::on_btn_save);
    connect(btn_clear_data_, &QPushButton::clicked, this, &SettingsWidget::on_btn_clear_data);

    // 测试 Webhook Ping
    connect(btn_test_ping_, &QPushButton::clicked, this, [this]() {
        QString url = webhook_url_edit_->text().trimmed();
        QString token = webhook_token_edit_->text().trimmed();
        int timeout = webhook_timeout_spin_->value();

        btn_test_ping_->setEnabled(false);
        btn_test_ping_->setText("探测中...");
        webhook_status_label_->setText("正在探测 Webhook 接收端网络连通性...");

        WebhookPusher::instance().test_ping(url, token, timeout, [this](bool success, int code, const QString& msg, qint64 latency) {
            btn_test_ping_->setEnabled(true);
            btn_test_ping_->setText("⚡ 测试 Ping");
            if (success) {
                webhook_status_label_->setText(QString("✅ 测试连通成功！响应码: HTTP %1, 往返耗时: %2 ms").arg(code).arg(latency));
                webhook_status_label_->setStyleSheet("color: #2ea043; font-size: 11px; padding: 4px 8px; background: rgba(46, 160, 67, 0.1); border-radius: 3px;");
            } else {
                webhook_status_label_->setText(QString("❌ 连通失败: %1 (耗时: %2 ms)").arg(msg).arg(latency));
                webhook_status_label_->setStyleSheet("color: #f85149; font-size: 11px; padding: 4px 8px; background: rgba(248, 81, 73, 0.1); border-radius: 3px;");
            }
        });
    });

    // 商户账号登录并拉取点位
    connect(btn_cloud_login_, &QPushButton::clicked, this, [this]() {
        QString url = cloud_url_edit_->text().trimmed();
        QString phone = cloud_phone_edit_->text().trimmed();
        QString pwd = cloud_password_edit_->text().trimmed();

        if (url.isEmpty() || phone.isEmpty() || pwd.isEmpty()) {
            QMessageBox::warning(this, "提示", "请填写云端服务地址、商户手机号及登录密码！");
            return;
        }

        btn_cloud_login_->setEnabled(false);
        btn_cloud_login_->setText("正在验证...");
        cloud_status_info_label_->setText("正在验证账号凭证并拉取业务点位资产...");

        EdgeReportManager::instance().login_with_credentials(url, phone, pwd, [this, phone](bool success, const QString& msg, const QJsonArray& points) {
            btn_cloud_login_->setEnabled(true);
            btn_cloud_login_->setText("🔐 登录并拉取点位");

            if (!success) {
                cloud_status_info_label_->setText(QString("❌ 登录失败: %1").arg(msg));
                cloud_status_info_label_->setStyleSheet("color: #f85149; font-size: 12px; padding: 7px 12px; background: rgba(248, 81, 73, 0.12); border-radius: 4px; border: 1px solid rgba(248, 81, 73, 0.4);");
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

            auto& cfg = ConfigManager::instance();
            cfg.set_cloud_user_phone(phone);
            cfg.save();

            cloud_status_info_label_->setText(QString("✅ 登录成功！商户账号 %1 获取到 %2 个已分配点位，请选择并绑定。").arg(phone).arg(points.size()));
            cloud_status_info_label_->setStyleSheet("color: #2ea043; font-size: 12px; font-weight: 500; padding: 7px 12px; background: rgba(46, 160, 67, 0.12); border-radius: 4px; border: 1px solid rgba(46, 160, 67, 0.4);");
        });
    });

    // 确认绑定点位
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

        auto& cfg = ConfigManager::instance();
        cfg.set_cloud_server_url(url);
        cfg.set_cloud_point_id(pid);
        cfg.set_cloud_point_name(pname);
        cfg.set_cloud_point_token(ptoken);
        cfg.set_cloud_client_name(client_name);
        cfg.save();

        // 绑定成功后自动触发拉取云端 RTSP 流与点位详细参数
        btn_sync_config_->click();
    });

    // 同步云端配置 (拉取云端管理员配置的 RTSP 视频流与分辨率)
    connect(btn_sync_config_, &QPushButton::clicked, this, [this, update_status_badge]() {
        btn_sync_config_->setEnabled(false);
        btn_sync_config_->setText("正在同步...");
        cloud_status_info_label_->setText("正在从云端拉取最新点位参数与摄像头 RTSP 配置...");

        EdgeReportManager::instance().fetch_cloud_config([this, update_status_badge](bool success, const QString& msg, const QJsonObject& cfg_data) {
            btn_sync_config_->setEnabled(true);
            btn_sync_config_->setText("🔄 同步云端配置 (RTSP流)");

            if (success) {
                QString pname = cfg_data.value("pointName").toString();
                QString rtsp = cfg_data.value("cameraRtsp").toString();
                QString phone = cfg_data.value("userPhone").toString();
                QString company = cfg_data.value("companyName").toString();

                auto& cfg = ConfigManager::instance();
                if (!pname.isEmpty()) cfg.set_cloud_point_name(pname);
                if (!phone.isEmpty()) cfg.set_cloud_user_phone(phone);
                if (!company.isEmpty()) cfg.set_cloud_company_name(company);

                if (!rtsp.trimmed().isEmpty()) {
                    video_input_edit_->setText(rtsp.trimmed());
                    cfg.set_video_source(rtsp.trimmed());
                    cfg.save();
                    emit video_source_changed(rtsp.trimmed());
                } else {
                    cfg.save();
                }

                update_status_badge();

                QString sync_info = QString("点位名称: %1\n所属商户: %2 (%3)\n摄像头流: %4")
                    .arg(pname)
                    .arg(company.isEmpty() ? "万科物业" : company)
                    .arg(phone.isEmpty() ? "13454118762" : phone)
                    .arg(rtsp.isEmpty() ? "未配置(保持本地视频源)" : rtsp);
                QMessageBox::information(this, "云端配置已同步", sync_info);
            } else {
                cloud_status_info_label_->setText(QString("❌ 同步云端配置失败: %1").arg(msg));
                cloud_status_info_label_->setStyleSheet("color: #f85149; font-size: 12px; font-weight: bold; padding: 7px 12px; background: rgba(248, 81, 73, 0.12); border-radius: 4px; border: 1px solid rgba(248, 81, 73, 0.4);");
                QMessageBox::warning(this, "同步失败", msg);
            }
        });
    });

    // 手动上报测试包
    connect(btn_test_report_, &QPushButton::clicked, this, [this]() {
        btn_test_report_->setEnabled(false);
        btn_test_report_->setText("正在上传...");
        cloud_status_info_label_->setText("正在向云端提交测试指标聚合包...");

        EdgeReportManager::instance().send_test_batch([this](bool success, const QString& msg, qint64 latency) {
            btn_test_report_->setEnabled(true);
            btn_test_report_->setText("⚡ 上报测试包");
            if (success) {
                cloud_status_info_label_->setText(QString("✅ 测试聚合包上报成功！%1 (耗时: %2 ms)").arg(msg).arg(latency));
                cloud_status_info_label_->setStyleSheet("color: #2ea043; font-size: 12px; font-weight: bold; padding: 7px 12px; background: rgba(46, 160, 67, 0.12); border-radius: 4px; border: 1px solid rgba(46, 160, 67, 0.4);");
            } else {
                cloud_status_info_label_->setText(QString("❌ 上报测试包失败: %1 (耗时: %2 ms)").arg(msg).arg(latency));
                cloud_status_info_label_->setStyleSheet("color: #f85149; font-size: 12px; font-weight: bold; padding: 7px 12px; background: rgba(248, 81, 73, 0.12); border-radius: 4px; border: 1px solid rgba(248, 81, 73, 0.4);");
            }
        });
    });
}

void SettingsWidget::on_btn_browse() {
    QString file = QFileDialog::getOpenFileName(this, "选择测试视频", "", "视频文件 (*.mp4 *.avi *.mkv);;所有文件 (*.*)");
    if (!file.isEmpty()) {
        video_input_edit_->setText(file);
    }
}

void SettingsWidget::on_btn_save() {
    auto& cfg = ConfigManager::instance();
    cfg.set_video_source(video_input_edit_->text().trimmed());
    cfg.set_clip_retention_days(retention_spin_->value());

    cfg.set_crowd_surge_enabled(surge_enabled_chk_->isChecked());
    cfg.set_crowd_surge_threshold(surge_threshold_spin_->value());
    cfg.set_crowd_surge_beep(surge_beep_chk_->isChecked());

    cfg.set_webhook_enabled(webhook_enabled_chk_->isChecked());
    cfg.set_webhook_url(webhook_url_edit_->text().trimmed());
    cfg.set_webhook_token(webhook_token_edit_->text().trimmed());
    cfg.set_webhook_timeout_ms(webhook_timeout_spin_->value());

    cfg.set_cloud_server_url(cloud_url_edit_->text().trimmed());
    cfg.set_cloud_client_name(cloud_client_name_edit_->text().trimmed());
    cfg.set_cloud_user_phone(cloud_phone_edit_->text().trimmed());

    cfg.save();

    emit video_source_changed(video_input_edit_->text().trimmed());
    emit settings_applied(surge_threshold_spin_->value(), surge_enabled_chk_->isChecked(), surge_beep_chk_->isChecked());
    QMessageBox::information(this, "成功", "设置已保存并生效。");
}

void SettingsWidget::on_btn_clear_data() {
    auto reply = QMessageBox::warning(
        this,
        "数据清理确认",
        "确定要清空全部通行记录与本地视频切片吗？\n\n"
        "• 数据库所有通行记录将被物理清除；\n"
        "• 本地录像切片文件将被全部删除；\n"
        "• 实时监控与今日统计数据将即刻归零。\n\n"
        "此操作不可撤销，是否确认继续？",
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

    // 2. 清空数据库记录 (passage_events, stats_hourly, offline_report_queue, VACUUM)
    int deleted_events = 0;
    bool db_ok = DatabaseManager::instance().clear_all_history_data(reset_rois, &deleted_events);

    // 3. 广播信号给 MainWindow 联动清空内存与界面
    emit data_cleared(reset_rois);

    double freed_mb = freed_bytes / (1024.0 * 1024.0);
    QString summary_msg = QString("数据清理完成：\n• 清除通行事实记录: %1 条\n• 删除录像切片: %2 个 (释放 %3 MB)")
        .arg(deleted_events).arg(deleted_files).arg(QString::number(freed_mb, 'f', 2));

    if (reset_rois) {
        summary_msg += "\n• 通道标定线已恢复初始状态";
    }

    QMessageBox::information(this, "清理完成", summary_msg);
}
