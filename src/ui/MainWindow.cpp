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
#include "network/EdgeReportManager.hpp"
#include <QTimer>
#include <QThreadPool>

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStatusBar>
#include <QApplication>
#include <QPainter>
#include <QCloseEvent>
#include <QBuffer>
#include <QDateTime>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("FlowSight 智能客流分析系统 v1.2");
    resize(1366, 800);
    setMinimumSize(1100, 680);

    // 初始化 SQLite 数据库与 WAL 模式
    DatabaseManager::instance().init_database("data/flowsight.db");

    // 初始化 Webhook 推送中心
    WebhookPusher::instance().init(this);

    // 初始化智能批量聚合与断网离线自愈上报管理器
    EdgeReportManager::instance().init(this);

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

    // 启动云端协同定时心跳: 每 5 秒向云端上报一次设备在线与遥测指标
    cloud_heartbeat_timer_ = new QTimer(this);
    connect(cloud_heartbeat_timer_, &QTimer::timeout, this, [this]() {
        EdgeReportManager::instance().send_heartbeat(
            latest_stats_.fps,
            latest_stats_.infer_ms,
            latest_stats_.current_occupancy,
            QString::fromStdString(latest_stats_.backend_name.empty() ? "CPU" : latest_stats_.backend_name)
        );
    });
    cloud_heartbeat_timer_->start(5000);

    setup_ui();
    setup_connections();

    // 冷启动从 SQLite 恢复今日累计统计与最新通行记录流水
    {
        int init_in = 0, init_out = 0;
        DatabaseManager::instance().get_today_counts(init_in, init_out);
        StatisticsSnapshot init_snap;
        init_snap.current_occupancy = 0;
        init_snap.today_entry = init_in;
        init_snap.today_exit = init_out;
        init_snap.today_total = init_in + init_out;
        view_monitor_->update_stats(init_snap);

        auto recent_events = DatabaseManager::instance().query_recent_events(50);
        view_monitor_->populate_recent_events(recent_events);
    }

    // 启动后台分析管线
    pipeline_ = std::make_unique<PipelineController>(this);

    // 绑定数据流到各个独立视图与网络推送
    connect(pipeline_.get(), &PipelineController::frame_ready, view_monitor_, &MonitorWidget::update_frame);
    connect(pipeline_.get(), &PipelineController::frame_ready, view_studio_, &StudioWidget::update_frame);

    // 现场快照预览：每 60 秒 (或首帧起动) 自动捕获当前带 ROI 绊线的水印快照上报云端
    auto last_snapshot_time = std::make_shared<qint64>(0);
    connect(pipeline_.get(), &PipelineController::frame_ready, this, [this, last_snapshot_time](const QImage& frame, const std::vector<TrackObject>& tracks) {
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - *last_snapshot_time < 60000 && *last_snapshot_time > 0) {
            return;
        }
        if (frame.isNull() || !EdgeReportManager::instance().is_cloud_online()) {
            return;
        }
        *last_snapshot_time = now;

        // 缩放并绘制轻量快照 (640x360 压缩到 ~30KB)
        QImage snap = frame.scaled(640, 360, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        {
            QPainter p(&snap);
            p.setRenderHint(QPainter::Antialiasing);

            auto rois = pipeline_->get_current_rois();
            float sx = static_cast<float>(snap.width()) / frame.width();
            float sy = static_cast<float>(snap.height()) / frame.height();
            for (const auto& roi : rois) {
                if (roi.vertices.size() >= 2) {
                    QPolygonF poly;
                    for (const auto& pt : roi.vertices) {
                        poly << QPointF(pt.x * sx, pt.y * sy);
                    }
                    p.setPen(QPen(QColor(0, 210, 255, 220), 2, Qt::SolidLine));
                    p.setBrush(QColor(0, 210, 255, 40));
                    p.drawPolygon(poly);
                }
            }

            p.setPen(QPen(QColor(0, 255, 128, 220)));
            p.setFont(QFont("Microsoft YaHei", 9, QFont::Bold));
            p.drawText(snap.rect().adjusted(0, 8, -10, 0), Qt::AlignRight | Qt::AlignTop, "● LIVE SNAPSHOT");
        }

        QByteArray bytes;
        QBuffer buf(&bytes);
        buf.open(QIODevice::WriteOnly);
        snap.save(&buf, "JPG", 75);

        EdgeReportManager::instance().upload_snapshot(bytes);
    });
    connect(pipeline_.get(), &PipelineController::stats_updated, view_monitor_, &MonitorWidget::update_stats);
    connect(pipeline_.get(), &PipelineController::stats_updated, this, &MainWindow::on_stats_updated);
    connect(pipeline_.get(), &PipelineController::event_occurred, view_monitor_, &MonitorWidget::add_passage_event);
    connect(pipeline_.get(), &PipelineController::event_occurred, view_evidence_, &EvidenceWidget::add_new_event);
    connect(pipeline_.get(), &PipelineController::event_occurred, this, [](const PassageEvent& ev) {
        EdgeReportManager::instance().record_passage(ev);
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
    connect(view_monitor_, &MonitorWidget::flow_direction_changed, this, [this](float angle, bool invert) {
        if (pipeline_) {
            pipeline_->set_roi_flow_direction(angle, invert);
        }
        auto rois = DatabaseManager::instance().load_latest_roi_configs();
        for (auto& r : rois) {
            r.flow_angle_deg = angle;
            r.invert_direction = invert;
        }
        DatabaseManager::instance().save_roi_configs(rois);
        if (view_studio_) {
            view_studio_->set_rois(rois);
        }
    });
    connect(view_settings_, &SettingsWidget::settings_applied, this, [this](int thresh, bool, bool) {
        if (view_monitor_) {
            view_monitor_->update_surge_threshold(thresh);
        }
    });
    connect(view_settings_, &SettingsWidget::data_cleared, this, [this](bool reset_rois) {
        if (pipeline_) {
            pipeline_->reset_today_counts();
        }
        if (view_monitor_) {
            view_monitor_->clear_display_records();
        }
        if (view_evidence_) {
            view_evidence_->refresh_events();
        }
        if (view_analytics_) {
            view_analytics_->refresh_report();
        }
        if (reset_rois) {
            auto saved_rois = DatabaseManager::instance().load_latest_roi_configs();
            if (view_monitor_) view_monitor_->update_active_rois(saved_rois);
            if (view_studio_) view_studio_->set_rois(saved_rois);
            if (pipeline_) pipeline_->update_roi_configs(saved_rois);
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
    auto* status_cloud = create_chip("☁️ 云端就绪", "#58a6ff");
    auto* status_queue = create_chip("📦 离线队列: 0", "#8b949e");

    statusBar()->addWidget(status_device);
    statusBar()->addWidget(status_infer_label_);
    statusBar()->addWidget(status_fps_label_);
    statusBar()->addWidget(status_wal);
    statusBar()->addWidget(status_cloud);
    statusBar()->addWidget(status_queue);

    connect(&EdgeReportManager::instance(), &EdgeReportManager::cloud_status_changed, this, [status_cloud](bool is_online, const QString& text) {
        if (text.contains("已注销") || text.contains("已下架") || text.contains("无效")) {
            status_cloud->setText("⚠️ 点位已下架(本地自治)");
            status_cloud->setStyleSheet("background-color: #3b2300; color: #e3b341; border: 1px solid #d29922; border-radius: 4px; padding: 2px 8px; margin: 0 4px; font-weight: 600;");
        } else {
            status_cloud->setText(is_online ? "☁️ 云端在线" : "☁️ 云端离线");
            status_cloud->setStyleSheet(QString(
                "background-color: #21262d; color: %1; border-radius: 4px; padding: 2px 8px; margin: 0 4px; font-weight: 500;"
            ).arg(is_online ? "#3fb950" : "#f85149"));
        }
        status_cloud->setToolTip(text);
    });

    connect(&EdgeReportManager::instance(), &EdgeReportManager::offline_queue_updated, this, [status_queue](int count) {
        status_queue->setText(QString("📦 离线队列: %1").arg(count));
        status_queue->setStyleSheet(QString(
            "background-color: #21262d; color: %1; border-radius: 4px; padding: 2px 8px; margin: 0 4px; font-weight: 500;"
        ).arg(count > 0 ? "#d29922" : "#8b949e"));
    });
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
    latest_stats_ = stats;
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
