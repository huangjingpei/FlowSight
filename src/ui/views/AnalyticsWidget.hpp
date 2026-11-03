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
    void on_datetime_changed(const QDateTime& dt);
    void on_filter_combo_changed();
    void on_btn_query_clicked();
    void on_btn_reset_clicked();
    void on_btn_export_csv();

private:
    void setup_ui();
    QWidget* create_kpi_card(const QString& title, QLabel*& out_val, QLabel*& out_sub, QLabel*& out_title);

    // 顶部综合筛选控件
    QComboBox* combo_preset_ = nullptr;
    QDateTimeEdit* dt_start_ = nullptr;
    QDateTimeEdit* dt_end_ = nullptr;
    QComboBox* combo_direction_ = nullptr;
    QLabel* lbl_gender_ = nullptr;
    QComboBox* combo_gender_ = nullptr;
    QPushButton* btn_query_ = nullptr;
    QPushButton* btn_reset_ = nullptr;
    QPushButton* btn_export_ = nullptr;

    // 核心概览指标卡片
    QLabel* card_title_total_ = nullptr;
    QLabel* total_flow_val_ = nullptr;
    QLabel* total_flow_sub_ = nullptr;

    QLabel* card_title_entry_ = nullptr;
    QLabel* entry_val_ = nullptr;
    QLabel* entry_sub_ = nullptr;

    QLabel* card_title_exit_ = nullptr;
    QLabel* exit_val_ = nullptr;
    QLabel* exit_sub_ = nullptr;

    QLabel* card_title_gender_ = nullptr;
    QLabel* gender_val_ = nullptr;
    QLabel* gender_sub_ = nullptr;

    QLabel* card_title_peak_ = nullptr;
    QLabel* peak_hour_val_ = nullptr;
    QLabel* peak_hour_sub_ = nullptr;

    // 选项卡与图表组件
    QTabWidget* tab_charts_ = nullptr;
    RealtimeTotalChartWidget* realtime_total_chart_ = nullptr;
    RealtimeAverageChartWidget* realtime_avg_chart_ = nullptr;
    AnalyticsChartWidget* hourly_chart_ = nullptr;

    bool is_live_today_view_ = true;
    bool is_updating_preset_ = false;
};
