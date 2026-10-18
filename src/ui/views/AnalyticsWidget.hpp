#pragma once
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>

class AnalyticsChartWidget;

class AnalyticsWidget : public QWidget {
    Q_OBJECT
public:
    explicit AnalyticsWidget(QWidget* parent = nullptr);
    void refresh_report();

private slots:
    void on_btn_export_csv();

private:
    void setup_ui();

    QComboBox* combo_range_ = nullptr;
    QPushButton* btn_export_ = nullptr;

    QLabel* total_flow_val_ = nullptr;
    QLabel* total_flow_sub_ = nullptr;
    QLabel* peak_hour_val_ = nullptr;
    QLabel* peak_hour_sub_ = nullptr;
    QLabel* gender_ratio_val_ = nullptr;
    QLabel* gender_ratio_sub_ = nullptr;
    QLabel* age_group_val_ = nullptr;
    QLabel* age_group_sub_ = nullptr;
    AnalyticsChartWidget* chart_widget_ = nullptr;
};
