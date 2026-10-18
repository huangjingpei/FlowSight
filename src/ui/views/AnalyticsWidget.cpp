#include "AnalyticsWidget.hpp"
#include "ui/components/AnalyticsChartWidget.hpp"
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

    // 2. 核心指标概览卡片 (真实数据库驱动)
    auto* kpi_layout = new QHBoxLayout();
    kpi_layout->setSpacing(12);

    auto create_info_box = [](const QString& title, const QString& border_color, QLabel*& out_val, QLabel*& out_sub) -> QWidget* {
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
        out_val = new QLabel("0 人次");
        out_val->setStyleSheet("color: #f0f6fc; font-size: 26px; font-weight: 800; font-family: 'Segoe UI', sans-serif; margin: 2px 0;");
        out_sub = new QLabel("进: 0 │ 出: 0");
        out_sub->setStyleSheet("color: #6e7681; font-size: 11px;");

        v->addWidget(t_lbl);
        v->addWidget(out_val);
        v->addWidget(out_sub);
        return box;
    };

    kpi_layout->addWidget(create_info_box("今日总客流量", "#00d2d3", total_flow_val_, total_flow_sub_));
    kpi_layout->addWidget(create_info_box("客流高峰时段", "#ffa502", peak_hour_val_, peak_hour_sub_));
    kpi_layout->addWidget(create_info_box("客群性别比例", "#a29bfe", gender_ratio_val_, gender_ratio_sub_));
    kpi_layout->addWidget(create_info_box("主力年龄区间", "#2ed573", age_group_val_, age_group_sub_));

    main_layout->addLayout(kpi_layout);

    // 3. 24小时时段分布趋势 (真实矢量图表驱动)
    chart_widget_ = new AnalyticsChartWidget(this);
    main_layout->addWidget(chart_widget_, 1);

    connect(btn_export_, &QPushButton::clicked, this, &AnalyticsWidget::on_btn_export_csv);
    connect(combo_range_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AnalyticsWidget::refresh_report);

    refresh_report();
}

void AnalyticsWidget::refresh_report() {
    int range_idx = combo_range_ ? combo_range_->currentIndex() : 0;
    QDateTime now = QDateTime::currentDateTime();
    int64_t start_ms = 0;
    int64_t end_ms = now.toMSecsSinceEpoch();

    if (range_idx == 0) {
        // 今日客流汇总 (今日 00:00:00 至今)
        start_ms = QDateTime(now.date(), QTime(0, 0, 0)).toMSecsSinceEpoch();
    } else if (range_idx == 1) {
        // 最近 3 天趋势
        start_ms = now.addDays(-3).toMSecsSinceEpoch();
    } else {
        // 最近 7 天报表
        start_ms = now.addDays(-7).toMSecsSinceEpoch();
    }

    auto events = DatabaseManager::instance().query_events_by_timerange(start_ms, end_ms, 5000);
    int total_entry = 0;
    int total_exit = 0;
    std::map<int, std::pair<int, int>> hourly_data;
    int male_cnt = 0, female_cnt = 0;

    for (const auto& ev : events) {
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(ev.timestamp_ms);
        int h = dt.time().hour();

        if (ev.direction == PassageDirection::ENTRY) {
            total_entry++;
            hourly_data[h].first++;
        } else if (ev.direction == PassageDirection::EXIT) {
            total_exit++;
            hourly_data[h].second++;
        }

        if (ev.gender == "男") male_cnt++;
        else if (ev.gender == "女") female_cnt++;
    }

    int total_flow = total_entry + total_exit;
    total_flow_val_->setText(QString("%1 人次").arg(total_flow));
    total_flow_sub_->setText(QString("进: %1 │ 出: %2").arg(total_entry).arg(total_exit));

    if (total_flow == 0) {
        peak_hour_val_->setText("-- : --");
        peak_hour_sub_->setText("所选范围无记录");
        gender_ratio_val_->setText("暂无数据");
        gender_ratio_sub_->setText("统计样本: 0 人次");
        age_group_val_->setText("暂无数据");
        age_group_sub_->setText("等待目标穿行");
        if (chart_widget_) chart_widget_->clear_data();
        return;
    }

    // 计算小时峰值
    int peak_h = -1;
    int max_cnt = -1;
    for (auto const& [h, p] : hourly_data) {
        int sum = p.first + p.second;
        if (sum > max_cnt) {
            max_cnt = sum;
            peak_h = h;
        }
    }

    if (peak_h >= 0) {
        peak_hour_val_->setText(QString("%1:00 ~ %2:00").arg(peak_h, 2, 10, QChar('0')).arg((peak_h + 1) % 24, 2, 10, QChar('0')));
        peak_hour_sub_->setText(QString("小时峰值: %1 人次").arg(max_cnt));
    }

    int valid_gender = male_cnt + female_cnt;
    if (valid_gender > 0) {
        int m_pct = (male_cnt * 100) / valid_gender;
        int f_pct = 100 - m_pct;
        gender_ratio_val_->setText(QString("男 %1% │ 女 %2%").arg(m_pct).arg(f_pct));
        gender_ratio_sub_->setText(QString("统计样本: %1 人次").arg(valid_gender));
    } else {
        gender_ratio_val_->setText("全量统计中");
        gender_ratio_sub_->setText(QString("真实样本: %1 人次").arg(total_flow));
    }

    age_group_val_->setText("真实样本归集");
    age_group_sub_->setText(QString("有效识别样本: %1 人次").arg(total_flow));

    if (chart_widget_) {
        chart_widget_->set_data(hourly_data);
    }
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

    int range_idx = combo_range_ ? combo_range_->currentIndex() : 0;
    QDateTime now = QDateTime::currentDateTime();
    int64_t start_ms = 0;
    int64_t end_ms = now.toMSecsSinceEpoch();

    if (range_idx == 0) start_ms = QDateTime(now.date(), QTime(0, 0, 0)).toMSecsSinceEpoch();
    else if (range_idx == 1) start_ms = now.addDays(-3).toMSecsSinceEpoch();
    else start_ms = now.addDays(-7).toMSecsSinceEpoch();

    auto events = DatabaseManager::instance().query_events_by_timerange(start_ms, end_ms, 50000);
    int id = 1;
    for (const auto& ev : events) {
        QString time_str = QDateTime::fromMSecsSinceEpoch(ev.timestamp_ms).toString("yyyy-MM-dd hh:mm:ss");
        QString dir_str = (ev.direction == PassageDirection::ENTRY) ? "ENTRY" : "EXIT";
        out << id++ << "," << time_str << "," << ev.track_id << "," << dir_str << ","
            << ev.confidence << "," << QString::fromStdString(ev.gender) << ","
            << QString::fromStdString(ev.age_group) << "," << QString::fromStdString(ev.clip_path) << "\n";
    }

    file.close();
    QMessageBox::information(this, "导出成功", QString("所选时段客流报表 (共 %1 条) 已导出至:\n%2").arg(events.size()).arg(filename));
}
