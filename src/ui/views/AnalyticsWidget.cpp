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
    main_layout->setContentsMargins(14, 14, 14, 14);
    main_layout->setSpacing(14);

    // 1. 顶部操作条
    auto* top_bar = new QHBoxLayout();
    auto* lbl_range = new QLabel("统计时间范围:", this);
    lbl_range->setStyleSheet("font-weight: bold; color: #c9d1d9;");
    top_bar->addWidget(lbl_range);

    combo_range_ = new QComboBox(this);
    combo_range_->setFixedWidth(160);
    combo_range_->addItems({"今日客流汇总", "最近 3 天趋势", "最近 7 天报表"});
    top_bar->addWidget(combo_range_);

    btn_export_ = new QPushButton("📥  导出 CSV 报表", this);
    btn_export_->setObjectName("btn_primary");
    btn_export_->setFixedHeight(34);
    top_bar->addWidget(btn_export_);
    top_bar->addStretch();
    main_layout->addLayout(top_bar);

    // 2. 核心指标概览卡片
    auto* kpi_layout = new QHBoxLayout();
    kpi_layout->setSpacing(12);

    auto create_info_box = [](const QString& title, const QString& val, const QString& sub, const QString& border_color) -> QWidget* {
        auto* box = new QWidget();
        box->setStyleSheet(QString(
            "background-color: #161b22; border: 1px solid #30363d; border-top: 3px solid %1; "
            "border-radius: 8px; padding: 12px;"
        ).arg(border_color));
        auto* v = new QVBoxLayout(box);
        v->setContentsMargins(4, 4, 4, 4);
        v->setSpacing(4);

        auto* t_lbl = new QLabel(title);
        t_lbl->setStyleSheet("color: #8b949e; font-size: 13px; font-weight: 500;");
        auto* v_lbl = new QLabel(val);
        v_lbl->setStyleSheet("color: #f0f6fc; font-size: 26px; font-weight: 800; font-family: 'Segoe UI', sans-serif; margin: 2px 0;");
        auto* s_lbl = new QLabel(sub);
        s_lbl->setStyleSheet("color: #6e7681; font-size: 11px;");

        v->addWidget(t_lbl);
        v->addWidget(v_lbl);
        v->addWidget(s_lbl);
        return box;
    };

    kpi_layout->addWidget(create_info_box("今日总客流量", "643 人次", "进: 328 | 出: 315", "#00d2d3"));
    kpi_layout->addWidget(create_info_box("客流高峰时段", "11:00 ~ 12:00", "小时峰值: 142 人次", "#ffa502"));
    kpi_layout->addWidget(create_info_box("客群性别比例", "男 54%  │  女 46%", "统计样本: 643人次", "#a29bfe"));
    kpi_layout->addWidget(create_info_box("主力年龄区间", "青年 (20 ~ 30)", "占比约 58.4%", "#2ed573"));

    main_layout->addLayout(kpi_layout);

    // 3. 24小时时段分布趋势
    auto* chart_group = new QGroupBox("24小时各时段进出流量分布趋势 (小时堆叠直方图)", this);
    auto* chart_vbox = new QVBoxLayout(chart_group);

    auto* mock_chart = new QLabel(
        "时间轴(时)  08:00    09:00    10:00    11:00(高峰) 12:00    13:00    14:00    15:00    16:00    17:00    18:00\n"
        "进入 [IN]   [██]     [████]   [██████] [████████]  [████]   [███]    [████]   [█████]  [██████] [█████]  [████]\n"
        "离开 [OUT]  [█ ]     [██  ]   [████  ] [███████ ]  [████]   [███]    [████]   [████ ]  [█████ ] [████ ]  [███ ]\n"
        "人次统计     28 人    65 人    112 人   142 人(峰)  88 人    54 人    76 人    98 人    110 人   92 人    80 人\n"
        "拥挤状态    ● 平稳   ● 适中   ▲ 繁忙   🔥 高峰     ● 适中   ● 平稳   ● 适中   ● 适中   ▲ 繁忙   ● 适中   ● 平稳",
        this
    );
    mock_chart->setStyleSheet(
        "background-color: #0d1117; color: #58a6ff; font-family: 'Consolas', 'Courier New', monospace; "
        "font-size: 13px; padding: 20px; border-radius: 6px; border: 1px solid #21262d; line-height: 200%;"
    );
    chart_vbox->addWidget(mock_chart);
    main_layout->addWidget(chart_group, 1);

    connect(btn_export_, &QPushButton::clicked, this, &AnalyticsWidget::on_btn_export_csv);
}

void AnalyticsWidget::refresh_report() {
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
    QMessageBox::information(this, "导出成功", QString("客流事实明细与切片报表已导出至:\n%1").arg(filename));
}
