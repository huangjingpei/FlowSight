#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QTabWidget>
#include "common/Types.hpp"
#include "storage/DatabaseManager.hpp"

class RealtimeTotalChartWidget;
class RealtimeAverageChartWidget;
class AnalyticsChartWidget;

class AnalyticsWidget : public QWidget {
    Q_OBJECT
public:
    explicit AnalyticsWidget(QWidget* parent = nullptr);
    ~AnalyticsWidget() override = default;

    // 刷新全量统计报表与指标
    void refresh_report();

    // 接收管线实时快照与穿行事件
    void update_realtime_stats(const StatisticsSnapshot& stats);
    void add_realtime_passage(const PassageEvent& ev);
    void reset_realtime_data();

private slots:
    void on_preset_changed(int index);
    void on_btn_query_clicked();
    void on_btn_reset_clicked();
    void on_btn_export_csv();

private:
    void setup_ui();
    QWidget* create_kpi_card(const QString& title, const QString& accent_color, QLabel*& out_val, QLabel*& out_sub);

    // 顶部综合筛选控件
    QComboBox* combo_preset_ = nullptr;
    QDateTimeEdit* dt_start_ = nullptr;
    QDateTimeEdit* dt_end_ = nullptr;
    QComboBox* combo_direction_ = nullptr;
    QComboBox* combo_gender_ = nullptr;
    QPushButton* btn_query_ = nullptr;
    QPushButton* btn_reset_ = nullptr;
    QPushButton* btn_export_ = nullptr;

    // 核心概览指标卡片
    QLabel* total_flow_val_ = nullptr;
    QLabel* total_flow_sub_ = nullptr;

    QLabel* entry_val_ = nullptr;
    QLabel* entry_sub_ = nullptr;

    QLabel* exit_val_ = nullptr;
    QLabel* exit_sub_ = nullptr;

    QLabel* gender_val_ = nullptr;
    QLabel* gender_sub_ = nullptr;

    QLabel* peak_hour_val_ = nullptr;
    QLabel* peak_hour_sub_ = nullptr;

    // 选项卡与图表组件
    QTabWidget* tab_charts_ = nullptr;
    RealtimeTotalChartWidget* realtime_total_chart_ = nullptr;
    RealtimeAverageChartWidget* realtime_avg_chart_ = nullptr;
    AnalyticsChartWidget* hourly_chart_ = nullptr;

    bool is_live_today_view_ = true;
};
