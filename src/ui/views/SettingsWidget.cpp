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
#include <QGridLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollArea>
#include <QFrame>

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
    main_layout->setContentsMargins(24, 18, 24, 20);
    main_layout->setSpacing(16);

    // 精准的设置页内部专属样式表，统一控件高度、间距并杜绝 QSpinBox 纵向拉伸黑框
    container->setStyleSheet(R"(
        QWidget#settings_container {
            background-color: #14161a;
        }
        QGroupBox {
            background-color: #181b21;
            border: 1px solid #282d37;
            border-radius: 6px;
            margin-top: 16px;
            padding: 16px 18px 14px 18px;
            font-weight: bold;
            font-size: 13px;
            color: #58a6ff;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 14px;
            padding: 0 8px;
            background-color: #181b21;
            border-radius: 3px;
        }
        QLabel {
            color: #c9d1d9;
            font-size: 12px;
        }
        QLabel.section-label {
            color: #8b949e;
            font-size: 12px;
            font-weight: 500;
        }
        QLineEdit, QComboBox {
            background-color: #131519;
            color: #e6edf3;
            border: 1px solid #30363d;
            border-radius: 4px;
            padding: 4px 10px;
            min-height: 28px;
            max-height: 32px;
            font-size: 12px;
        }
        QLineEdit:focus, QComboBox:focus {
            border: 1px solid #388bfd;
            background-color: #16181f;
        }
        QSpinBox {
            background-color: #131519;
            color: #e6edf3;
            border: 1px solid #30363d;
            border-radius: 4px;
            padding: 3px 6px;
            min-height: 28px;
            max-height: 30px;
            font-size: 12px;
        }
        QSpinBox:focus {
            border: 1px solid #388bfd;
        }
        QPushButton {
            background-color: #21262d;
            color: #c9d1d9;
            border: 1px solid #30363d;
            border-radius: 4px;
            padding: 5px 14px;
            min-height: 28px;
            max-height: 32px;
            font-size: 12px;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: #30363d;
            color: #ffffff;
            border-color: #8b949e;
        }
        QPushButton:pressed {
            background-color: #161b22;
        }
        QCheckBox {
            color: #c9d1d9;
            font-size: 12px;
            spacing: 6px;
        }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border-radius: 3px;
            border: 1px solid #30363d;
            background-color: #131519;
        }
        QCheckBox::indicator:checked {
            background-color: #1f6feb;
            border-color: #388bfd;
        }
    )");

    auto& cfg = ConfigManager::instance();

    // =========================================================================
    // 1. 视频输入源设置 (卡片 1)
    // =========================================================================
    auto* src_group = new QGroupBox("🎬 视频输入源与码流接入", container);
    auto* src_layout = new QVBoxLayout(src_group);
    src_layout->setContentsMargins(16, 16, 16, 16);
    src_layout->setSpacing(10);

    auto* src_row1 = new QHBoxLayout();
    src_row1->setSpacing(10);
    auto* src_lbl = new QLabel("视频源 / RTSP流地址:", src_group);
    src_lbl->setFixedWidth(135);
    src_row1->addWidget(src_lbl);

    video_input_edit_ = new QLineEdit(cfg.get_video_source(), src_group);
    video_input_edit_->setPlaceholderText("支持本地 MP4/AVI 视频文件路径 或 工业网络摄像机 rtsp:// 流地址");
    src_row1->addWidget(video_input_edit_, 1);

    btn_browse_ = new QPushButton("📁 浏览文件...", src_group);
    btn_browse_->setCursor(Qt::PointingHandCursor);
    src_row1->addWidget(btn_browse_);
    src_layout->addLayout(src_row1);

    // 快捷通道填充辅助条
    auto* src_preset_layout = new QHBoxLayout();
    src_preset_layout->setSpacing(8);
    auto* preset_tip = new QLabel("快速通道选择:", src_group);
    preset_tip->setFixedWidth(135);
    preset_tip->setStyleSheet("color: #8b949e; font-size: 11px;");
    src_preset_layout->addWidget(preset_tip);

    auto* btn_demo_clip = new QPushButton("🎞️ 载入测试走廊视频 (configs/test_corridor.mp4)", src_group);
    btn_demo_clip->setStyleSheet("color: #58a6ff; background: rgba(56, 139, 253, 0.08); border: 1px dashed #30363d; font-size: 11px; padding: 3px 10px; min-height: 24px; max-height: 26px;");
    btn_demo_clip->setCursor(Qt::PointingHandCursor);
    connect(btn_demo_clip, &QPushButton::clicked, this, [this]() {
        video_input_edit_->setText("configs/test_corridor.mp4");
    });
    src_preset_layout->addWidget(btn_demo_clip);

    auto* btn_demo_rtsp = new QPushButton("🌐 填入云端已配置的 RTSP 流地址", src_group);
    btn_demo_rtsp->setStyleSheet("color: #3fb950; background: rgba(46, 160, 67, 0.08); border: 1px dashed #30363d; font-size: 11px; padding: 3px 10px; min-height: 24px; max-height: 26px;");
    btn_demo_rtsp->setCursor(Qt::PointingHandCursor);
    connect(btn_demo_rtsp, &QPushButton::clicked, this, [this]() {
        btn_sync_config_->click();
    });
    src_preset_layout->addWidget(btn_demo_rtsp);
    src_preset_layout->addStretch();
    src_layout->addLayout(src_preset_layout);

    main_layout->addWidget(src_group);

    // =========================================================================
    // 2. 云端平台与点位管理 (卡片 2 - 结构化分区)
    // =========================================================================
    auto* cloud_group = new QGroupBox("☁️ 云端协同中台与点位资产", container);
    auto* cloud_layout = new QVBoxLayout(cloud_group);
    cloud_layout->setContentsMargins(16, 16, 16, 16);
    cloud_layout->setSpacing(12);

    // 行 1: 云端服务中台地址与快捷填入
    auto* cloud_row1 = new QHBoxLayout();
    cloud_row1->setSpacing(10);
    auto* url_lbl = new QLabel("云端中台 API URL:", cloud_group);
    url_lbl->setFixedWidth(135);
    cloud_row1->addWidget(url_lbl);

    QString default_url = cfg.get_cloud_server_url().trimmed();
    while (default_url.endsWith('/')) default_url.chop(1);
    if (default_url.startsWith("http://flowsight.1t90.com", Qt::CaseInsensitive)) {
        default_url.replace(0, 4, "https");
    }
    if (default_url.isEmpty() || default_url == "http://localhost:8080") {
        default_url = "https://flowsight.1t90.com";
    }
    cloud_url_edit_ = new QLineEdit(default_url, cloud_group);
    cloud_url_edit_->setPlaceholderText("例如: https://flowsight.1t90.com 或 http://124.221.179.44:8088");
    cloud_row1->addWidget(cloud_url_edit_, 1);

    auto* btn_url_prod = new QPushButton("🌐 官方生产网关", cloud_group);
    btn_url_prod->setStyleSheet("font-size: 11px; padding: 3px 10px;");
    btn_url_prod->setCursor(Qt::PointingHandCursor);
    connect(btn_url_prod, &QPushButton::clicked, this, [this]() {
        cloud_url_edit_->setText("https://flowsight.1t90.com");
    });
    cloud_row1->addWidget(btn_url_prod);

    auto* btn_url_local = new QPushButton("🏠 本地回环地址", cloud_group);
    btn_url_local->setStyleSheet("font-size: 11px; padding: 3px 10px;");
    btn_url_local->setCursor(Qt::PointingHandCursor);
    connect(btn_url_local, &QPushButton::clicked, this, [this]() {
        cloud_url_edit_->setText("http://127.0.0.1:8088");
    });
    cloud_row1->addWidget(btn_url_local);
    cloud_layout->addLayout(cloud_row1);

    // 行 2: 商户账号登录与点位拉取
    auto* cloud_row2 = new QHBoxLayout();
    cloud_row2->setSpacing(10);
    auto* phone_lbl = new QLabel("商户登录手机号:", cloud_group);
    phone_lbl->setFixedWidth(135);
    cloud_row2->addWidget(phone_lbl);

    cloud_phone_edit_ = new QLineEdit(cfg.get_cloud_user_phone(), cloud_group);
    cloud_phone_edit_->setPlaceholderText("请输入租户手机号 (如 13454118763)");
    cloud_row2->addWidget(cloud_phone_edit_, 1);

    auto* pwd_lbl = new QLabel("登录密码:", cloud_group);
    pwd_lbl->setFixedWidth(65);
    cloud_row2->addWidget(pwd_lbl);

    cloud_password_edit_ = new QLineEdit(cloud_group);
    cloud_password_edit_->setEchoMode(QLineEdit::Password);
    cloud_password_edit_->setPlaceholderText("请输入商户登录密码");
    cloud_row2->addWidget(cloud_password_edit_, 1);

    btn_cloud_login_ = new QPushButton("🔐 登录并拉取点位", cloud_group);
    btn_cloud_login_->setCursor(Qt::PointingHandCursor);
    btn_cloud_login_->setStyleSheet(
        "QPushButton { background: #1f6feb; color: #ffffff; font-weight: bold; padding: 6px 14px; border: 1px solid #388bfd; border-radius: 4px; }"
        "QPushButton:hover { background: #388bfd; }"
    );
    cloud_row2->addWidget(btn_cloud_login_);
    cloud_layout->addLayout(cloud_row2);

    // 分隔线
    auto* hline = new QFrame(cloud_group);
    hline->setFrameShape(QFrame::HLine);
    hline->setStyleSheet("background-color: #21262d; border: none; max-height: 1px; margin: 2px 0;");
    cloud_layout->addWidget(hline);

    // 行 3: 关联业务点位选择与双向同步
    auto* cloud_row3 = new QHBoxLayout();
    cloud_row3->setSpacing(10);
    auto* pt_lbl = new QLabel("选择关联通道点位:", cloud_group);
    pt_lbl->setFixedWidth(135);
    cloud_row3->addWidget(pt_lbl);

    cloud_point_combo_ = new QComboBox(cloud_group);
    cloud_point_combo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    if (cfg.get_cloud_point_id() > 0) {
        QVariantMap pdata;
        pdata["id"] = cfg.get_cloud_point_id();
        pdata["name"] = cfg.get_cloud_point_name();
        pdata["token"] = cfg.get_cloud_point_token();
        cloud_point_combo_->addItem(QString("[%1] %2").arg(cfg.get_cloud_point_id()).arg(cfg.get_cloud_point_name()), pdata);
    } else {
        cloud_point_combo_->addItem("⚪ 未绑定点位 (请先点击上方登录拉取已授权点位)");
    }
    cloud_row3->addWidget(cloud_point_combo_, 1);

    btn_bind_point_ = new QPushButton("🔗 确认绑定", cloud_group);
    btn_bind_point_->setCursor(Qt::PointingHandCursor);
    btn_bind_point_->setStyleSheet(
        "QPushButton { background: #238636; color: #ffffff; font-weight: bold; padding: 6px 14px; border: 1px solid #2ea043; border-radius: 4px; }"
        "QPushButton:hover { background: #2ea043; }"
    );
    cloud_row3->addWidget(btn_bind_point_);

    btn_sync_config_ = new QPushButton("🔄 同步云端配置 (RTSP流)", cloud_group);
    btn_sync_config_->setCursor(Qt::PointingHandCursor);
    btn_sync_config_->setStyleSheet(
        "QPushButton { background: #1f6feb; color: #ffffff; font-weight: 500; padding: 6px 14px; border: 1px solid #388bfd; border-radius: 4px; }"
        "QPushButton:hover { background: #388bfd; }"
    );
    cloud_row3->addWidget(btn_sync_config_);

    btn_test_report_ = new QPushButton("⚡ 上报测试包", cloud_group);
    btn_test_report_->setCursor(Qt::PointingHandCursor);
    btn_test_report_->setStyleSheet(
        "QPushButton { background: #6e40c9; color: #ffffff; font-weight: 500; padding: 6px 14px; border: 1px solid #8957e5; border-radius: 4px; }"
        "QPushButton:hover { background: #8957e5; }"
    );
    cloud_row3->addWidget(btn_test_report_);
    cloud_layout->addLayout(cloud_row3);

    // 备用隐藏兼容控件
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

    // 行 4: 现代科技感状态面板
    cloud_status_info_label_ = new QLabel(cloud_group);
    auto update_status_badge = [this]() {
        auto& cfg = ConfigManager::instance();
        if (cfg.get_cloud_point_id() > 0 && !cfg.get_cloud_point_token().isEmpty()) {
            QString company = cfg.get_cloud_company_name();
            QString phone = cfg.get_cloud_user_phone();
            QString company_display = company.isEmpty() ? "已授权商户" : company;
            QString phone_display = phone.isEmpty() ? "--" : phone;
            QString bizTag = cfg.is_vehicle_mode() ? "🚗 车流感知" : "🚶 客流感知";
            cloud_status_info_label_->setText(
                QString("🟢 通道运行状态: 在线 (Point-ID: %1)   │   点位: %2 [%3]   │   所属租户: %4 (%5)")
                .arg(cfg.get_cloud_point_id())
                .arg(cfg.get_cloud_point_name())
                .arg(bizTag)
                .arg(company_display)
                .arg(phone_display)
            );
            cloud_status_info_label_->setStyleSheet(
                "color: #3fb950; font-weight: 600; font-size: 12px; padding: 8px 14px; background: rgba(46, 160, 67, 0.12); border-radius: 4px; border-left: 3px solid #3fb950; border-top: 1px solid rgba(46,160,67,0.3); border-right: 1px solid rgba(46,160,67,0.3); border-bottom: 1px solid rgba(46,160,67,0.3);"
            );
        } else {
            cloud_status_info_label_->setText("⚪ 未绑定通道点位：请使用管理员下发的商户账号登录后选择点位绑定");
            cloud_status_info_label_->setStyleSheet(
                "color: #8b949e; font-size: 12px; padding: 8px 14px; background: rgba(139, 148, 158, 0.08); border-radius: 4px; border-left: 3px solid #8b949e; border-top: 1px solid rgba(139,148,158,0.2); border-right: 1px solid rgba(139,148,158,0.2); border-bottom: 1px solid rgba(139,148,158,0.2);"
            );
        }
    };
    update_status_badge();
    cloud_layout->addWidget(cloud_status_info_label_);

    main_layout->addWidget(cloud_group);

    // =========================================================================
    // 3. 告警风控与网络推送 (卡片 3 - 规整双行网格)
    // =========================================================================
    auto* alert_group = new QGroupBox("🔔 业务风控与事件通知推送", container);
    auto* alert_layout = new QVBoxLayout(alert_group);
    alert_layout->setContentsMargins(16, 16, 16, 16);
    alert_layout->setSpacing(12);

    // 行 1: 客流超载告警设置
    auto* alert_row1 = new QHBoxLayout();
    alert_row1->setSpacing(14);
    surge_enabled_chk_ = new QCheckBox("启用区域人员超员风控告警", alert_group);
    surge_enabled_chk_->setChecked(cfg.get_crowd_surge_enabled());
    surge_enabled_chk_->setStyleSheet("color: #f85149; font-weight: bold; font-size: 12px;");
    alert_row1->addWidget(surge_enabled_chk_);

    alert_row1->addSpacing(10);
    alert_row1->addWidget(new QLabel("报警超员阈值:", alert_group));
    surge_threshold_spin_ = new QSpinBox(alert_group);
    surge_threshold_spin_->setRange(1, 500);
    surge_threshold_spin_->setValue(cfg.get_crowd_surge_threshold());
    surge_threshold_spin_->setSuffix(" 人");
    surge_threshold_spin_->setFixedWidth(85);
    alert_row1->addWidget(surge_threshold_spin_);

    alert_row1->addSpacing(10);
    surge_beep_chk_ = new QCheckBox("开启声音蜂鸣警报", alert_group);
    surge_beep_chk_->setChecked(cfg.get_crowd_surge_beep());
    alert_row1->addWidget(surge_beep_chk_);
    alert_row1->addStretch();
    alert_layout->addLayout(alert_row1);

    // 行 2: Webhook 事件推送总线
    auto* alert_row2 = new QHBoxLayout();
    alert_row2->setSpacing(10);
    webhook_enabled_chk_ = new QCheckBox("启用 Webhook 实时推送", alert_group);
    webhook_enabled_chk_->setChecked(cfg.get_webhook_enabled());
    webhook_enabled_chk_->setStyleSheet("color: #58a6ff; font-weight: 500; font-size: 12px;");
    webhook_enabled_chk_->setFixedWidth(160);
    alert_row2->addWidget(webhook_enabled_chk_);

    alert_row2->addWidget(new QLabel("推送 URL:", alert_group));
    webhook_url_edit_ = new QLineEdit(cfg.get_webhook_url(), alert_group);
    webhook_url_edit_->setPlaceholderText("http://127.0.0.1:8088/api/v1/events");
    alert_row2->addWidget(webhook_url_edit_, 2);

    alert_row2->addWidget(new QLabel("Token:", alert_group));
    webhook_token_edit_ = new QLineEdit(cfg.get_webhook_token(), alert_group);
    webhook_token_edit_->setPlaceholderText("可选 Bearer Token");
    alert_row2->addWidget(webhook_token_edit_, 1);

    alert_row2->addWidget(new QLabel("超时:", alert_group));
    webhook_timeout_spin_ = new QSpinBox(alert_group);
    webhook_timeout_spin_->setRange(500, 15000);
    webhook_timeout_spin_->setSingleStep(500);
    webhook_timeout_spin_->setValue(cfg.get_webhook_timeout_ms());
    webhook_timeout_spin_->setSuffix(" ms");
    webhook_timeout_spin_->setFixedWidth(90);
    alert_row2->addWidget(webhook_timeout_spin_);

    btn_test_ping_ = new QPushButton("⚡ 测试 Ping", alert_group);
    btn_test_ping_->setCursor(Qt::PointingHandCursor);
    alert_row2->addWidget(btn_test_ping_);
    alert_layout->addLayout(alert_row2);

    webhook_status_label_ = new QLabel("就绪 - 点击「测试 Ping」可检验远端服务器网络连通性", alert_group);
    webhook_status_label_->setStyleSheet("color: #8b949e; font-size: 11px; padding: 5px 10px; background: rgba(139, 148, 158, 0.08); border-radius: 4px;");
    alert_layout->addWidget(webhook_status_label_);

    main_layout->addWidget(alert_group);

    // =========================================================================
    // 4. 存储策略与数据维护 (卡片 4 - 根除纵向拉伸黑框)
    // =========================================================================
    auto* maint_group = new QGroupBox("💾 本地存储与系统维护策略", container);
    auto* maint_layout = new QVBoxLayout(maint_group);
    maint_layout->setContentsMargins(16, 16, 16, 16);
    maint_layout->setSpacing(10);

    auto* maint_row1 = new QHBoxLayout();
    maint_row1->setSpacing(10);

    auto* ret_lbl = new QLabel("录像切片保留天数:", maint_group);
    maint_row1->addWidget(ret_lbl);

    retention_spin_ = new QSpinBox(maint_group);
    retention_spin_->setRange(1, 365);
    retention_spin_->setValue(cfg.get_clip_retention_days());
    retention_spin_->setSuffix(" 天");
    retention_spin_->setFixedWidth(85);
    maint_row1->addWidget(retention_spin_);

    auto* ret_hint = new QLabel("(先进先出 FIFO 循环自动覆盖清理，超期自动删除)", maint_group);
    ret_hint->setStyleSheet("color: #8b949e; font-size: 11px;");
    maint_row1->addWidget(ret_hint);

    maint_row1->addStretch();

    reset_rois_chk_ = new QCheckBox("同时重置防区通道标定线", maint_group);
    reset_rois_chk_->setStyleSheet("color: #8b949e; font-size: 12px;");
    maint_row1->addWidget(reset_rois_chk_);

    btn_clear_data_ = new QPushButton("⚠️ 清空本地历史数据与录像切片", maint_group);
    btn_clear_data_->setCursor(Qt::PointingHandCursor);
    btn_clear_data_->setStyleSheet(
        "QPushButton { background: #8c1d18; color: #ffffff; font-weight: bold; font-size: 12px; padding: 6px 14px; border-radius: 4px; border: 1px solid #da3633; }"
        "QPushButton:hover { background: #b3231c; border-color: #f85149; }"
        "QPushButton:pressed { background: #6e1410; }"
    );
    maint_row1->addWidget(btn_clear_data_);
    maint_layout->addLayout(maint_row1);

    auto* maint_tip = new QLabel("💡 提示: 边缘录像存储采用智能循环队列，当磁盘空间不足 15% 或超过保留天数时，系统将自动物理释放最久远的切片，保障工控机无需人工清盘长期稳定运行。", maint_group);
    maint_tip->setStyleSheet("color: #8b949e; font-size: 11px;");
    maint_layout->addWidget(maint_tip);

    main_layout->addWidget(maint_group);

    // 弹性支撑推顶
    main_layout->addStretch(1);

    // =========================================================================
    // 5. 底部操作栏 (硬件状态与保存控制)
    // =========================================================================
    auto* bottom_bar = new QWidget(container);
    auto* bottom_layout = new QHBoxLayout(bottom_bar);
    bottom_layout->setContentsMargins(0, 8, 0, 0);
    bottom_layout->setSpacing(12);

    hardware_info_label_ = new QLabel("⚡ CPU 多线程推理引擎已就绪  │  SQLite WAL 高性能存盘模式  │  断网边缘自愈运行中", bottom_bar);
    hardware_info_label_->setStyleSheet("color: #8c96a3; font-size: 11px; font-weight: 500;");
    bottom_layout->addWidget(hardware_info_label_);

    bottom_layout->addStretch();

    auto* btn_reload = new QPushButton("🔄 放弃修改", bottom_bar);
    btn_reload->setCursor(Qt::PointingHandCursor);
    btn_reload->setToolTip("重新从配置文件恢复未保存的设置");
    connect(btn_reload, &QPushButton::clicked, this, [this]() {
        auto& cfg = ConfigManager::instance();
        video_input_edit_->setText(cfg.get_video_source());
        retention_spin_->setValue(cfg.get_clip_retention_days());
        surge_enabled_chk_->setChecked(cfg.get_crowd_surge_enabled());
        surge_threshold_spin_->setValue(cfg.get_crowd_surge_threshold());
        surge_beep_chk_->setChecked(cfg.get_crowd_surge_beep());
        webhook_enabled_chk_->setChecked(cfg.get_webhook_enabled());
        webhook_url_edit_->setText(cfg.get_webhook_url());
        webhook_token_edit_->setText(cfg.get_webhook_token());
        webhook_timeout_spin_->setValue(cfg.get_webhook_timeout_ms());
        cloud_url_edit_->setText(cfg.get_cloud_server_url());
        cloud_phone_edit_->setText(cfg.get_cloud_user_phone());
        QMessageBox::information(this, "提示", "已从本地配置恢复上次保存的设置！");
    });
    bottom_layout->addWidget(btn_reload);

    btn_save_ = new QPushButton("💾 保存并应用全部设置", bottom_bar);
    btn_save_->setCursor(Qt::PointingHandCursor);
    btn_save_->setStyleSheet(
        "QPushButton { padding: 8px 28px; background: #1f6feb; color: #ffffff; font-weight: bold; font-size: 13px; border-radius: 4px; border: 1px solid #388bfd; min-height: 34px; }"
        "QPushButton:hover { background: #388bfd; }"
        "QPushButton:pressed { background: #1158c7; }"
    );
    bottom_layout->addWidget(btn_save_);

    main_layout->addWidget(bottom_bar);

    scroll_area->setWidget(container);
    root_layout->addWidget(scroll_area);

    // =========================================================================
    // 信号槽逻辑绑定
    // =========================================================================
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
                webhook_status_label_->setStyleSheet("color: #3fb950; font-size: 11px; padding: 5px 10px; background: rgba(46, 160, 67, 0.1); border-radius: 4px; border: 1px solid rgba(46, 160, 67, 0.3);");
            } else {
                webhook_status_label_->setText(QString("❌ 连通失败: %1 (耗时: %2 ms)").arg(msg).arg(latency));
                webhook_status_label_->setStyleSheet("color: #f85149; font-size: 11px; padding: 5px 10px; background: rgba(248, 81, 73, 0.1); border-radius: 4px; border: 1px solid rgba(248, 81, 73, 0.3);");
            }
        });
    });

    // 商户账号登录并拉取点位
    connect(btn_cloud_login_, &QPushButton::clicked, this, [this]() {
        QString url = cloud_url_edit_->text().trimmed();
        while (url.endsWith('/')) url.chop(1);
        if (url.startsWith("http://flowsight.1t90.com", Qt::CaseInsensitive)) {
            url.replace(0, 4, "https");
        }
        cloud_url_edit_->setText(url);

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
                cloud_status_info_label_->setStyleSheet("color: #f85149; font-size: 12px; padding: 8px 14px; background: rgba(248, 81, 73, 0.12); border-radius: 4px; border-left: 3px solid #f85149;");
                return;
            }

            cloud_point_combo_->clear();
            QString my_client = ConfigManager::instance().get_cloud_client_name();

            for (const auto& val : points) {
                QJsonObject p = val.toObject();
                qint64 pid = p["id"].toInteger();
                QString pname = p["pointName"].toString();
                QString pcode = p["pointCode"].toString();
                QString ptoken = p["pointToken"].toString();
                bool isOccupied = p["isOccupied"].toBool();
                QString boundClient = p["boundClientName"].toString();

                QString bizType = p["bizType"].toString();
                if (bizType.isEmpty()) bizType = "PEDESTRIAN";

                QVariantMap map;
                map["id"] = pid;
                map["name"] = pname;
                map["token"] = ptoken;
                map["occupied"] = isOccupied;
                map["boundClient"] = boundClient;
                map["bizType"] = bizType;

                QString statusTag = "🟢 空闲";
                if (isOccupied) {
                    if (!boundClient.isEmpty() && boundClient == my_client) {
                        statusTag = "🟢 本机已绑";
                    } else {
                        statusTag = QString("🔴 设备【%1】占用中").arg(boundClient.isEmpty() ? "其他节点" : boundClient);
                    }
                }

                QString bizTag = (bizType == "VEHICLE") ? "🚗 车流" : "🚶 客流";
                cloud_point_combo_->addItem(QString("[%1] %2 (%3) [%4] - %5").arg(pid).arg(pname).arg(pcode).arg(bizTag).arg(statusTag), map);
            }

            auto& cfg = ConfigManager::instance();
            cfg.set_cloud_user_phone(phone);
            cfg.save();

            cloud_status_info_label_->setText(QString("✅ 登录成功！商户账号 %1 获取到 %2 个已分配点位，请选择并绑定。").arg(phone).arg(points.size()));
            cloud_status_info_label_->setStyleSheet("color: #3fb950; font-size: 12px; font-weight: 500; padding: 8px 14px; background: rgba(46, 160, 67, 0.12); border-radius: 4px; border-left: 3px solid #3fb950;");
        });
    });

    // 确认绑定点位 (支持独占排他锁核验与换绑自动解绑握手)
    connect(btn_bind_point_, &QPushButton::clicked, this, [this, update_status_badge]() {
        if (cloud_point_combo_->count() == 0) {
            QMessageBox::warning(this, "提示", "请先登录云端拉取点位列表！");
            return;
        }

        QVariantMap map = cloud_point_combo_->currentData().toMap();
        qint64 pid = map["id"].toLongLong();
        QString pname = map["name"].toString();
        QString ptoken = map["token"].toString();
        bool isOccupied = map["occupied"].toBool();
        QString boundClient = map["boundClient"].toString();
        QString url = cloud_url_edit_->text().trimmed();
        while (url.endsWith('/')) url.chop(1);
        if (url.startsWith("http://flowsight.1t90.com", Qt::CaseInsensitive)) {
            url.replace(0, 4, "https");
        }
        cloud_url_edit_->setText(url);
        QString client_name = ConfigManager::instance().get_cloud_client_name();

        bool force = false;
        if (isOccupied && !boundClient.isEmpty() && boundClient != client_name) {
            auto reply = QMessageBox::warning(
                this,
                "点位独占排他冲突警告",
                QString("⚠️ 点位【%1】当前正在被另一台边缘设备【%2】独占在线运行！\n\n"
                        "为防止数据重复与采集通道错乱，同一物理点位禁止多机重复绑定。\n"
                        "若原设备已下线或需要强制换绑至本机，请点击【Yes】强制解除旧设备锁定。")
                    .arg(pname).arg(boundClient),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No
            );
            if (reply != QMessageBox::Yes) {
                return;
            }
            force = true;
        }

        btn_bind_point_->setEnabled(false);
        btn_bind_point_->setText("正在绑定...");

        EdgeReportManager::instance().bind_point(url, pid, pname, ptoken, client_name, force, [this, update_status_badge, map](bool success, const QString& msg) {
            btn_bind_point_->setEnabled(true);
            btn_bind_point_->setText("🔗 确认绑定");

            if (success) {
                update_status_badge();
                QString bound_biz = map["bizType"].toString();
                if (!bound_biz.isEmpty()) {
                    emit biz_type_changed(bound_biz);
                }
                // 绑定成功后自动触发拉取云端 RTSP 流与点位详细参数
                btn_sync_config_->click();
            } else {
                cloud_status_info_label_->setText(QString("❌ 绑定失败: %1").arg(msg));
                cloud_status_info_label_->setStyleSheet("color: #f85149; font-size: 12px; font-weight: bold; padding: 8px 14px; background: rgba(248, 81, 73, 0.12); border-radius: 4px; border-left: 3px solid #f85149;");
                QMessageBox::warning(this, "绑定失败", msg);
            }
        });
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
                QString bizType = cfg_data.value("bizType").toString();

                auto& cfg = ConfigManager::instance();
                if (!pname.isEmpty()) cfg.set_cloud_point_name(pname);
                if (!phone.isEmpty()) cfg.set_cloud_user_phone(phone);
                if (!company.isEmpty()) cfg.set_cloud_company_name(company);
                if (!bizType.isEmpty()) {
                    cfg.set_biz_type(bizType);
                    emit biz_type_changed(bizType);
                }

                if (!rtsp.trimmed().isEmpty()) {
                    video_input_edit_->setText(rtsp.trimmed());
                    cfg.set_video_source(rtsp.trimmed());
                    cfg.save();
                    emit video_source_changed(rtsp.trimmed());
                } else {
                    cfg.save();
                }

                update_status_badge();

                QString bizDesc = (bizType == "VEHICLE") ? "🚗 车流感知 (车辆与车牌识别)" : "🚶 客流感知 (人数与性别年龄)";
                QString sync_info = QString("点位名称: %1\n感知模式: %2\n所属商户: %3 (%4)\n摄像头流: %5")
                    .arg(pname)
                    .arg(bizDesc)
                    .arg(company.isEmpty() ? "已授权商户" : company)
                    .arg(phone.isEmpty() ? "--" : phone)
                    .arg(rtsp.isEmpty() ? "未配置(保持本地视频源)" : rtsp);
                QMessageBox::information(this, "云端配置已同步", sync_info);
            } else {
                cloud_status_info_label_->setText(QString("❌ 同步云端配置失败: %1").arg(msg));
                cloud_status_info_label_->setStyleSheet("color: #f85149; font-size: 12px; font-weight: bold; padding: 8px 14px; background: rgba(248, 81, 73, 0.12); border-radius: 4px; border-left: 3px solid #f85149;");
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
                cloud_status_info_label_->setStyleSheet("color: #3fb950; font-size: 12px; font-weight: bold; padding: 8px 14px; background: rgba(46, 160, 67, 0.12); border-radius: 4px; border-left: 3px solid #3fb950;");
            } else {
                cloud_status_info_label_->setText(QString("❌ 上报测试包失败: %1 (耗时: %2 ms)").arg(msg).arg(latency));
                cloud_status_info_label_->setStyleSheet("color: #f85149; font-size: 12px; font-weight: bold; padding: 8px 14px; background: rgba(248, 81, 73, 0.12); border-radius: 4px; border-left: 3px solid #f85149;");
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
    QString new_video_source = video_input_edit_->text().trimmed();
    cfg.set_video_source(new_video_source);
    cfg.set_clip_retention_days(retention_spin_->value());

    cfg.set_crowd_surge_enabled(surge_enabled_chk_->isChecked());
    cfg.set_crowd_surge_threshold(surge_threshold_spin_->value());
    cfg.set_crowd_surge_beep(surge_beep_chk_->isChecked());

    cfg.set_webhook_enabled(webhook_enabled_chk_->isChecked());
    cfg.set_webhook_url(webhook_url_edit_->text().trimmed());
    cfg.set_webhook_token(webhook_token_edit_->text().trimmed());
    cfg.set_webhook_timeout_ms(webhook_timeout_spin_->value());

    cfg.set_cloud_server_url(cloud_url_edit_->text().trimmed());
    cfg.set_cloud_user_phone(cloud_phone_edit_->text().trimmed());

    cfg.save();

    emit video_source_changed(new_video_source);
    emit settings_applied(surge_threshold_spin_->value(), surge_enabled_chk_->isChecked(), surge_beep_chk_->isChecked());

    // 双向同步回写：若当前已绑定云端点位，将修改后的视频流/点位配置同步更新至云端数据库
    if (cfg.get_cloud_point_id() > 0 && !cfg.get_cloud_point_token().isEmpty()) {
        EdgeReportManager::instance().update_cloud_point_config(
            cfg.get_cloud_point_name(),
            new_video_source,
            [this](bool success, const QString& msg) {
                if (success) {
                    qInfo() << "[SettingsWidget] 双向同步完成:" << msg;
                } else {
                    qWarning() << "[SettingsWidget] 双向同步提示:" << msg;
                }
            }
        );
        QMessageBox::information(this, "成功", "设置已保存！已生效并双向同步更新至云端中台点位资产。");
    } else {
        QMessageBox::information(this, "成功", "设置已保存至本地配置并生效。");
    }
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
