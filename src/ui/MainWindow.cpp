#include "MainWindow.hpp"
#include "views/MonitorWidget.hpp"
#include "views/StudioWidget.hpp"
#include "views/EvidenceWidget.hpp"
#include "views/AnalyticsWidget.hpp"
#include "views/SettingsWidget.hpp"
#include "storage/DatabaseManager.hpp"
#include "storage/ConfigManager.hpp"
#include "storage/StoragePruner.hpp"
#include "network/WebhookPusher.hpp"
#include <QTimer>
#include <QThreadPool>

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStatusBar>
#include <QApplication>
#include <QPainter>
#include <QCloseEvent>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("FlowSight 智能客流分析系统 v1.2");
    resize(1366, 800);
    setMinimumSize(1100, 680);

    // 初始化 SQLite 数据库与 WAL 模式
    DatabaseManager::instance().init_database("data/flowsight.db");

    // 初始化 Webhook 推送中心
    WebhookPusher::instance().init(this);

    // 异步执行磁盘保护清理 (按配置天数清理过期短视频切片，非阻塞杜绝 UI 卡顿)
    int retention_days = ConfigManager::instance().get_clip_retention_days();
    QThreadPool::globalInstance()->start([retention_days]() {
        StoragePruner::prune_expired_clips("clips", retention_days);
    });

    // 启动后台定时器: 每 6 小时自动巡检一次磁盘切片
    auto* prune_timer = new QTimer(this);
    connect(prune_timer, &QTimer::timeout, this, []() {
        int days = ConfigManager::instance().get_clip_retention_days();
        QThreadPool::globalInstance()->start([days]() {
            StoragePruner::prune_expired_clips("clips", days);
        });
    });
    prune_timer->start(6 * 3600 * 1000);

    setup_ui();
    setup_connections();

    // 启动后台分析管线
    pipeline_ = std::make_unique<PipelineController>(this);

    // 绑定数据流到各个独立视图与网络推送
    connect(pipeline_.get(), &PipelineController::frame_ready, view_monitor_, &MonitorWidget::update_frame);
    connect(pipeline_.get(), &PipelineController::frame_ready, view_studio_, &StudioWidget::update_frame);
    connect(pipeline_.get(), &PipelineController::stats_updated, view_monitor_, &MonitorWidget::update_stats);
    connect(pipeline_.get(), &PipelineController::stats_updated, this, &MainWindow::on_stats_updated);
    connect(pipeline_.get(), &PipelineController::event_occurred, view_monitor_, &MonitorWidget::add_passage_event);
    connect(pipeline_.get(), &PipelineController::event_occurred, view_evidence_, &EvidenceWidget::add_new_event);
    connect(pipeline_.get(), &PipelineController::event_occurred, this, [](const PassageEvent& ev) {
        WebhookPusher::instance().push_event(ev);
    });
    connect(pipeline_.get(), &PipelineController::crowd_surge_status_changed, view_monitor_, &MonitorWidget::set_crowd_surge_alert);
    connect(pipeline_.get(), &PipelineController::crowd_surge_status_changed, this, [](bool is_surged, int occ, int thresh) {
        WebhookPusher::instance().push_surge_alert(is_surged, occ, thresh);
    });
    connect(pipeline_.get(), &PipelineController::rois_auto_initialized, this, [this](const std::vector<RoiConfig>& rois) {
        view_monitor_->update_active_rois(rois);
        view_studio_->set_rois(rois);
    });

    connect(pipeline_.get(), &PipelineController::media_info_ready, view_monitor_, &MonitorWidget::set_media_info);
    connect(pipeline_.get(), &PipelineController::position_changed, view_monitor_, &MonitorWidget::set_playback_position);
    connect(pipeline_.get(), &PipelineController::playback_state_changed, view_monitor_, &MonitorWidget::set_playback_state);

    auto* ctrl = view_monitor_->control_bar();
    if (ctrl) {
        connect(ctrl, &PlaybackControlBar::play_requested, pipeline_.get(), &PipelineController::play);
        connect(ctrl, &PlaybackControlBar::pause_requested, pipeline_.get(), &PipelineController::pause);
        connect(ctrl, &PlaybackControlBar::stop_requested, pipeline_.get(), &PipelineController::stop);
        connect(ctrl, &PlaybackControlBar::seek_requested, pipeline_.get(), &PipelineController::seek);
        connect(ctrl, &PlaybackControlBar::step_forward_requested, pipeline_.get(), &PipelineController::step_forward);
        connect(ctrl, &PlaybackControlBar::step_backward_requested, pipeline_.get(), &PipelineController::step_backward);
        connect(ctrl, &PlaybackControlBar::volume_changed, pipeline_.get(), &PipelineController::set_volume);
        connect(ctrl, &PlaybackControlBar::mute_toggled, pipeline_.get(), &PipelineController::set_muted);
        connect(ctrl, &PlaybackControlBar::rate_changed, pipeline_.get(), &PipelineController::set_playback_rate);
    }

    connect(view_monitor_, &MonitorWidget::request_change_video, this, [this](const QString& path) {
        if (pipeline_) {
            pipeline_->set_video_source(path.toStdString());
        }
    });
    connect(view_settings_, &SettingsWidget::video_source_changed, this, [this](const QString& path) {
        if (pipeline_) {
            pipeline_->set_video_source(path.toStdString());
        }
    });
    connect(view_settings_, &SettingsWidget::settings_applied, this, [this](int thresh, bool, bool) {
        if (view_monitor_) {
            view_monitor_->update_surge_threshold(thresh);
        }
    });

    std::string init_video = ConfigManager::instance().get_video_source().toStdString();
    pipeline_->start_pipeline("models/yolov8n.onnx", init_video);

    // 冷启动持久化恢复: 将 SQLite 中所有保存的 ROI 注入分析管线、监控面板与工作室
    auto saved_rois = DatabaseManager::instance().load_latest_roi_configs();
    if (!saved_rois.empty()) {
        view_monitor_->update_active_rois(saved_rois);
        view_studio_->set_rois(saved_rois);
        pipeline_->update_roi_configs(saved_rois);
    }
}

MainWindow::~MainWindow() = default;

void MainWindow::setup_ui() {
    auto* central_widget = new QWidget(this);
    auto* main_layout = new QHBoxLayout(central_widget);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);

    // 1. 左侧标准安防 NVR 导航栏容器 (含品牌 Logo 头与硬件状态脚)
    auto* sidebar_container = new QWidget(this);
    sidebar_container->setFixedWidth(160);
    sidebar_container->setStyleSheet("background-color: #181b20; border-right: 1px solid #23272e;");
    auto* sidebar_vbox = new QVBoxLayout(sidebar_container);
    sidebar_vbox->setContentsMargins(0, 0, 0, 10);
    sidebar_vbox->setSpacing(0);

    // 顶部 Logo 标识区
    auto* brand_widget = new QWidget(this);
    brand_widget->setFixedHeight(64);
    brand_widget->setStyleSheet("border-bottom: 1px solid #23272e; background-color: #181b20;");
    auto* brand_layout = new QVBoxLayout(brand_widget);
    brand_layout->setContentsMargins(14, 10, 10, 10);
    brand_layout->setSpacing(2);

    auto* brand_title = new QLabel("FLOWSIGHT", brand_widget);
    brand_title->setStyleSheet("color: #388bfd; font-weight: 900; font-size: 15px; letter-spacing: 1px;");
    auto* brand_sub = new QLabel("NVR 客流分析控制台", brand_widget);
    brand_sub->setStyleSheet("color: #8c96a3; font-size: 11px;");
    brand_layout->addWidget(brand_title);
    brand_layout->addWidget(brand_sub);
    sidebar_vbox->addWidget(brand_widget);

    // 导航列表项
    nav_sidebar_ = new QListWidget(this);
    nav_sidebar_->setObjectName("nav_sidebar");
    nav_sidebar_->setFocusPolicy(Qt::NoFocus);

    nav_sidebar_->addItem("📹  实时监控");
    nav_sidebar_->addItem("📐  空间标定");
    nav_sidebar_->addItem("🎬  证据回溯");
    nav_sidebar_->addItem("📊  数据报表");
    nav_sidebar_->addItem("⚙️  系统设置");
    nav_sidebar_->setCurrentRow(0);

    sidebar_vbox->addWidget(nav_sidebar_, 1);

    // 侧边栏底部硬件标签
    auto* gpu_badge = new QWidget(this);
    gpu_badge->setStyleSheet("background-color: #1f2329; border: 1px solid #282c34; border-radius: 3px; margin: 0 8px; padding: 4px;");
    auto* gpu_layout = new QVBoxLayout(gpu_badge);
    gpu_layout->setContentsMargins(6, 4, 6, 4);
    gpu_badge_label_ = new QLabel("● 推理引擎探测中...", gpu_badge);
    gpu_badge_label_->setStyleSheet("color: #58a6ff; font-size: 11px; font-weight: bold;");
    gpu_layout->addWidget(gpu_badge_label_);
    sidebar_vbox->addWidget(gpu_badge);

    // 2. 右侧多页面堆栈容器
    stacked_views_ = new QStackedWidget(this);

    view_monitor_ = new MonitorWidget(this);
    view_studio_ = new StudioWidget(this);
    view_evidence_ = new EvidenceWidget(this);
    view_analytics_ = new AnalyticsWidget(this);
    view_settings_ = new SettingsWidget(this);

    stacked_views_->addWidget(view_monitor_);    // Index 0
    stacked_views_->addWidget(view_studio_);     // Index 1
    stacked_views_->addWidget(view_evidence_);   // Index 2
    stacked_views_->addWidget(view_analytics_);  // Index 3
    stacked_views_->addWidget(view_settings_);   // Index 4

    main_layout->addWidget(sidebar_container);
    main_layout->addWidget(stacked_views_);
    setCentralWidget(central_widget);

    // 3. 底部状态栏现代芯片设计
    auto create_chip = [this](const QString& text, const QString& color) -> QLabel* {
        auto* chip = new QLabel(text, this);
        chip->setStyleSheet(QString(
            "background-color: #21262d; color: %1; border-radius: 4px; padding: 2px 8px; margin: 0 4px; font-weight: 500;"
        ).arg(color));
        return chip;
    };

    auto* status_device = create_chip("● 设备在线", "#3fb950");
    status_infer_label_ = create_chip("⚡ 推理: 14.2 ms", "#58a6ff");
    status_fps_label_ = create_chip("📺 帧率: 30.0 FPS", "#d2a8ff");
    auto* status_wal = create_chip("💾 WAL模式已就绪", "#7ee787");

    statusBar()->addWidget(status_device);
    statusBar()->addWidget(status_infer_label_);
    statusBar()->addWidget(status_fps_label_);
    statusBar()->addWidget(status_wal);
}

void MainWindow::setup_connections() {
    connect(nav_sidebar_, &QListWidget::currentRowChanged, this, &MainWindow::on_nav_item_changed);
    connect(view_studio_, &StudioWidget::rois_saved, this, &MainWindow::on_rois_calibrated);

    // 首次载入 ROI 到标定页与监控页
    auto saved_rois = DatabaseManager::instance().load_latest_roi_configs();
    if (!saved_rois.empty()) {
        view_monitor_->update_active_rois(saved_rois);
        view_studio_->set_rois(saved_rois);
    }
}

void MainWindow::on_nav_item_changed(int row) {
    stacked_views_->setCurrentIndex(row);
    if (row == 2) {
        view_evidence_->refresh_events();
    } else if (row == 3) {
        view_analytics_->refresh_report();
    }
}

void MainWindow::on_roi_calibrated(const RoiConfig& new_roi) {
    on_rois_calibrated({new_roi});
}

void MainWindow::on_rois_calibrated(const std::vector<RoiConfig>& new_rois) {
    DatabaseManager::instance().save_roi_configs(new_rois);
    if (pipeline_) {
        pipeline_->update_roi_configs(new_rois);
    }
    view_monitor_->update_active_rois(new_rois);
}

void MainWindow::on_stats_updated(const StatisticsSnapshot& stats) {
    QString backend_str = QString::fromStdString(stats.backend_name);
    status_infer_label_->setText(QString("⚡ 推理: %1 ms (%2)")
        .arg(QString::number(stats.infer_ms, 'f', 1))
        .arg(backend_str.contains("CUDA") ? "CUDA" : "CPU"));
    status_fps_label_->setText(QString("📺 帧率: %1 FPS").arg(QString::number(stats.fps, 'f', 1)));

    if (gpu_badge_label_) {
        if (backend_str.contains("CUDA") || backend_str.contains("GPU")) {
            gpu_badge_label_->setText("● NVIDIA GPU (CUDA)");
            gpu_badge_label_->setStyleSheet("color: #3fb950; font-size: 11px; font-weight: bold;");
        } else {
            gpu_badge_label_->setText("● CPU 多线程高性能模式");
            gpu_badge_label_->setStyleSheet("color: #58a6ff; font-size: 11px; font-weight: bold;");
        }
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (pipeline_) {
        pipeline_->stop();
    }
    event->accept();
}
