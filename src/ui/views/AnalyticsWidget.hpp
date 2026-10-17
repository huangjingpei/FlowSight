#pragma once
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>

class AnalyticsWidget : public QWidget {
    Q_OBJECT
public:
    explicit AnalyticsWidget(QWidget* parent = nullptr);

private slots:
    void on_btn_export_csv();
    void refresh_report();

private:
    void setup_ui();

    QComboBox* combo_range_ = nullptr;
    QPushButton* btn_export_ = nullptr;
    QLabel* summary_kpi_label_ = nullptr;
};
