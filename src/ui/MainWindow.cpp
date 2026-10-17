#include "MainWindow.hpp"
#include "views/MonitorWidget.hpp"
#include "views/StudioWidget.hpp"
#include "views/EvidenceWidget.hpp"
#include "views/AnalyticsWidget.hpp"
#include "views/SettingsWidget.hpp"
#include "storage/DatabaseManager.hpp"

#include <QHBoxLayout>
#include <QStatusBar>
#include <QApplication>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("FlowSight 智能客流分析系统 v1.2");
    resize(1280, 768);
    setMinimumSize(1024, 600);

    // 初始化 SQLite 数据库与 WAL 模式
    DatabaseManager::instance().init_database("data/flowsight.db");

    setup_ui();
    setup_connections();

    // 启动后台分析管线
    pipeline_ = std::make_unique<PipelineController>(this);

    // 绑定数据流到各个独立视图
    connect(pipeline_.get(), &PipelineController::frame_ready, view_monitor_, &MonitorWidget::update_frame);
    connect(pipeline_.get(), &PipelineController::frame_ready, view_studio_, &StudioWidget::update_frame);
    connect(pipeline_.get(), &PipelineController::stats_updated, view_monitor_, &MonitorWidget::update_stats);
    connect(pipeline_.get(), &PipelineController::stats_updated, this, &MainWindow::on_stats_updated);
    connect(pipeline_.get(), &PipelineController::event_occurred, view_monitor_, &MonitorWidget::add_passage_event);
    connect(pipeline_.get(), &PipelineController::event_occurred, view_evidence_, &EvidenceWidget::add_new_event);

    pipeline_->start_pipeline("models/yolov8n.onnx", "configs/test_corridor.mp4");
}

MainWindow::~MainWindow() = default;

void MainWindow::setup_ui() {
    auto* central_widget = new QWidget(this);
    auto* main_layout = new QHBoxLayout(central_widget);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);

    // 1. 左侧现代化导航栏
    nav_sidebar_ = new QListWidget(this);
    nav_sidebar_->setFixedWidth(120);
    nav_sidebar_->setSpacing(8);
    nav_sidebar_->setStyleSheet(
        "QListWidget { background-color: #1e272e; border: none; color: #d2dae2; font-size: 14px; padding-top: 10px; } "
        "QListWidget::item { height: 48px; border-left: 4px solid transparent; padding-left: 12px; } "
        "QListWidget::item:selected { background-color: #2f3640; color: #00d2d3; border-left: 4px solid #00d2d3; font-weight: bold; } "
        "QListWidget::item:hover:!selected { background-color: #2a313d; }"
    );

    nav_sidebar_->addItem("实时监控");
    nav_sidebar_->addItem("空间标定");
    nav_sidebar_->addItem("证据回溯");
    nav_sidebar_->addItem("数据报表");
    nav_sidebar_->addItem("系统设置");
    nav_sidebar_->setCurrentRow(0);

    // 2. 右侧多页面堆栈容器
    stacked_views_ = new QStackedWidget(this);
    stacked_views_->setStyleSheet("QStackedWidget { background-color: #181c20; }");

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

    main_layout->addWidget(nav_sidebar_);
    main_layout->addWidget(stacked_views_);
    setCentralWidget(central_widget);

    // 3. 底部状态栏
    status_infer_label_ = new QLabel("推理: 初始化中...", this);
    status_fps_label_ = new QLabel("帧率: 0.0 FPS", this);
    statusBar()->setStyleSheet("QStatusBar { background-color: #1e272e; color: #808e9b; font-size: 12px; }");
    statusBar()->addWidget(new QLabel(" 设备状态: 正常运行 | ", this));
    statusBar()->addWidget(status_infer_label_);
    statusBar()->addWidget(new QLabel(" | ", this));
    statusBar()->addWidget(status_fps_label_);
}

void MainWindow::setup_connections() {
    connect(nav_sidebar_, &QListWidget::currentRowChanged, this, &MainWindow::on_nav_item_changed);
    connect(view_studio_, &StudioWidget::roi_saved, this, &MainWindow::on_roi_calibrated);

    // 首次载入 ROI 到标定页与监控页
    auto saved_rois = DatabaseManager::instance().load_latest_roi_configs();
    if (!saved_rois.empty()) {
        view_monitor_->update_active_roi(saved_rois.front());
        view_studio_->set_roi(saved_rois.front());
    }
}

void MainWindow::on_nav_item_changed(int row) {
    stacked_views_->setCurrentIndex(row);
    if (row == 2) {
        view_evidence_->refresh_events();
    }
}

void MainWindow::on_roi_calibrated(const RoiConfig& new_roi) {
    if (pipeline_) {
        pipeline_->update_roi_config(new_roi);
    }
    view_monitor_->update_active_roi(new_roi);
}

void MainWindow::on_stats_updated(const StatisticsSnapshot& stats) {
    status_infer_label_->setText(QString("推理延迟: %1 ms").arg(QString::number(stats.infer_ms, 'f', 1)));
    status_fps_label_->setText(QString("视频帧率: %1 FPS").arg(QString::number(stats.fps, 'f', 1)));
}
