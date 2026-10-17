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
    void on_stats_updated(const StatisticsSnapshot& stats);

private:
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

    std::unique_ptr<PipelineController> pipeline_;
};
