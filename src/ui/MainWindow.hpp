#pragma once
#include <QMainWindow>
#include <QStackedWidget>
#include <QListWidget>
#include <QLabel>
#include <memory>
#include "pipeline/PipelineController.hpp"

class MonitorWidget;
class StudioWidget;
class EvidenceWidget;
class AnalyticsWidget;
class SettingsWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_nav_item_changed(int row);
    void on_roi_calibrated(const RoiConfig& new_roi);
    void on_rois_calibrated(const std::vector<RoiConfig>& new_rois);
    void on_stats_updated(const StatisticsSnapshot& stats);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void cleanup_on_shutdown();
    bool is_shutting_down_ = false;

    void setup_ui();
    void setup_connections();

    QListWidget* nav_sidebar_ = nullptr;
    QStackedWidget* stacked_views_ = nullptr;

    MonitorWidget* view_monitor_ = nullptr;
    StudioWidget* view_studio_ = nullptr;
    EvidenceWidget* view_evidence_ = nullptr;
    AnalyticsWidget* view_analytics_ = nullptr;
    SettingsWidget* view_settings_ = nullptr;

    QLabel* status_infer_label_ = nullptr;
    QLabel* status_fps_label_ = nullptr;
    QLabel* gpu_badge_label_ = nullptr;
    QLabel* brand_title_ = nullptr;
    QLabel* brand_sub_ = nullptr;

    StatisticsSnapshot latest_stats_;
    QTimer* cloud_heartbeat_timer_ = nullptr;
    QTimer* prune_timer_ = nullptr;

    std::unique_ptr<PipelineController> pipeline_;
};
