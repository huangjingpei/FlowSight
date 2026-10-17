#include "AnalyticsWidget.hpp"
#include "storage/DatabaseManager.hpp"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QMessageBox>

AnalyticsWidget::AnalyticsWidget(QWidget* parent) : QWidget(parent) {
    setup_ui();
}

void AnalyticsWidget::setup_ui() {
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(12, 12, 12, 12);
    main_layout->setSpacing(12);

    // 1. 顶部操作条
    auto* top_bar = new QHBoxLayout();
    top_bar->addWidget(new QLabel("时间跨度:", this));

    combo_range_ = new QComboBox(this);
    combo_range_->addItems({"今日客流", "最近 3 天", "最近 7 天"});
    top_bar->addWidget(combo_range_);

    btn_export_ = new QPushButton("导出 CSV 报表", this);
    btn_export_->setStyleSheet("padding: 6px 12px; background: #27ae60; color: white; font-weight: bold; border-radius: 4px;");
    top_bar->addWidget(btn_export_);
    top_bar->addStretch();
    main_layout->addLayout(top_bar);

    // 2. 核心指标概览卡片
    auto* kpi_group = new QGroupBox("客流与画像核心指标", this);
    auto* kpi_layout = new QHBoxLayout(kpi_group);

    auto create_info_box = [](const QString& title, const QString& val, const QString& sub) -> QWidget* {
        auto* box = new QWidget();
        box->setStyleSheet("background: #2f3640; border-radius: 6px; padding: 12px;");
        auto* v = new QVBoxLayout(box);
        auto* t_lbl = new QLabel(title);
        t_lbl->setStyleSheet("color: #dcdde1; font-size: 13px;");
        auto* v_lbl = new QLabel(val);
        v_lbl->setStyleSheet("color: #f5f6fa; font-size: 26px; font-weight: bold; margin: 4px 0;");
        auto* s_lbl = new QLabel(sub);
        s_lbl->setStyleSheet("color: #718093; font-size: 12px;");
        v->addWidget(t_lbl);
        v->addWidget(v_lbl);
        v->addWidget(s_lbl);
        return box;
    };

    kpi_layout->addWidget(create_info_box("今日总客流量", "643 人次", "进: 328 | 出: 315"));
    kpi_layout->addWidget(create_info_box("客流高峰时段", "11:00 ~ 12:00", "小时峰值: 142 人次"));
    kpi_layout->addWidget(create_info_box("性别画像比例", "男 54% | 女 46%", "基准采样样本: 643"));
    kpi_layout->addWidget(create_info_box("主流年龄客群", "青年 (20-30)", "占比约 58%"));

    main_layout->addWidget(kpi_group);

    // 3. 24小时时段分布趋势模拟图
    auto* chart_group = new QGroupBox("24小时客流时段吞吐趋势 (柱状堆叠分布)", this);
    auto* chart_vbox = new QVBoxLayout(chart_group);

    auto* mock_chart = new QLabel(
        "时段(时)  08:00  09:00  10:00  11:00(峰)  12:00  13:00  14:00  15:00  16:00  17:00  18:00\n"
        "进入[IN]   [██]   [████] [██████][████████] [████] [███]  [████] [█████][██████][█████][████]\n"
        "离开[OUT]  [█ ]   [██  ] [████  ][███████ ] [████] [███]  [████] [████ ][█████ ][████ ][███ ]\n"
        "人次统计    28     65     112     142       88     54     76     98     110    92     80",
        this
    );
    mock_chart->setStyleSheet("background: #1e272e; color: #00d2d3; font-family: Consolas, monospace; "
                             "font-size: 14px; padding: 20px; border-radius: 6px; line-height: 180%;");
    chart_vbox->addWidget(mock_chart);
    main_layout->addWidget(chart_group, 1);

    connect(btn_export_, &QPushButton::clicked, this, &AnalyticsWidget::on_btn_export_csv);
}

void AnalyticsWidget::refresh_report() {
    // 刷新逻辑
}

void AnalyticsWidget::on_btn_export_csv() {
    QDir dir("exports");
    if (!dir.exists()) dir.mkpath(".");

    QString filename = QString("exports/report_%1.csv").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "错误", "无法创建 CSV 报表文件!");
        return;
    }

    QTextStream out(&file);
    out << "EventID,Timestamp,TrackID,Direction,Confidence,Gender,AgeGroup,ClipPath\n";

    auto events = DatabaseManager::instance().query_recent_events(500);
    int id = 1;
    for (const auto& ev : events) {
        QString time_str = QDateTime::fromMSecsSinceEpoch(ev.timestamp_ms).toString("yyyy-MM-dd hh:mm:ss");
        QString dir_str = (ev.direction == PassageDirection::ENTRY) ? "ENTRY" : "EXIT";
        out << id++ << "," << time_str << "," << ev.track_id << "," << dir_str << ","
            << ev.confidence << "," << QString::fromStdString(ev.gender) << ","
            << QString::fromStdString(ev.age_group) << "," << QString::fromStdString(ev.clip_path) << "\n";
    }

    file.close();
    QMessageBox::information(this, "成功", QString("CSV 报表已成功导出至:\n%1").arg(filename));
}
