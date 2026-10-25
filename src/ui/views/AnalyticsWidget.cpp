#include "AnalyticsWidget.hpp"
#include "ui/components/RealtimeTotalChartWidget.hpp"
#include "ui/components/RealtimeAverageChartWidget.hpp"
#include "ui/components/AnalyticsChartWidget.hpp"
#include "storage/DatabaseManager.hpp"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QMessageBox>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <algorithm>

AnalyticsWidget::AnalyticsWidget(QWidget* parent) : QWidget(parent) {
    setup_ui();
}

void AnalyticsWidget::setup_ui() {
    auto* root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->setSpacing(0);

    auto* scroll_area = new QScrollArea(this);
    scroll_area->setWidgetResizable(true);
    scroll_area->setFrameShape(QFrame::NoFrame);
    scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_area->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto* container = new QWidget(scroll_area);
    container->setObjectName("analytics_container");
    auto* main_layout = new QVBoxLayout(container);
    main_layout->setContentsMargins(14, 14, 14, 14);
    main_layout->setSpacing(14);

    // ==========================================
    // 1. 顶部操作与综合检索筛选条
    // ==========================================
    auto* filter_card = new QWidget(container);
    filter_card->setStyleSheet(
        "background-color: #161b22; border: 1px solid #30363d; border-radius: 8px; padding: 6px 10px;"
    );
    auto* filter_layout = new QHBoxLayout(filter_card);
    filter_layout->setContentsMargins(6, 4, 6, 4);
    filter_layout->setSpacing(6);

    // 时间预设
    auto* lbl_preset = new QLabel("统计时段:", filter_card);
    lbl_preset->setStyleSheet("font-weight: bold; color: #c9d1d9;");
    filter_layout->addWidget(lbl_preset);

    combo_preset_ = new QComboBox(filter_card);
    combo_preset_->setFixedWidth(115);
    combo_preset_->addItems({"今日客流汇总", "最近 3 天趋势", "最近 7 天报表", "自定义时段"});
    filter_layout->addWidget(combo_preset_);

    // 起始时间
    auto* lbl_from = new QLabel("从:", filter_card);
    lbl_from->setStyleSheet("color: #8b949e;");
    filter_layout->addWidget(lbl_from);

    QDateTime now = QDateTime::currentDateTime();
    dt_start_ = new QDateTimeEdit(filter_card);
    dt_start_->setDisplayFormat("yyyy-MM-dd HH:mm");
    dt_start_->setCalendarPopup(true);
    dt_start_->setFixedWidth(140);
    dt_start_->setDateTime(QDateTime(now.date(), QTime(0, 0, 0)));
    filter_layout->addWidget(dt_start_);

    // 结束时间
    auto* lbl_to = new QLabel("至:", filter_card);
    lbl_to->setStyleSheet("color: #8b949e;");
    filter_layout->addWidget(lbl_to);

    dt_end_ = new QDateTimeEdit(filter_card);
    dt_end_->setDisplayFormat("yyyy-MM-dd HH:mm");
    dt_end_->setCalendarPopup(true);
    dt_end_->setFixedWidth(140);
    dt_end_->setDateTime(now);
    filter_layout->addWidget(dt_end_);

    // 穿行方向筛选
    auto* lbl_dir = new QLabel("方向:", filter_card);
    lbl_dir->setStyleSheet("font-weight: bold; color: #c9d1d9;");
    filter_layout->addWidget(lbl_dir);

    combo_direction_ = new QComboBox(filter_card);
    combo_direction_->setFixedWidth(100);
    combo_direction_->addItem("全部方向", -1);
    combo_direction_->addItem("🟢 仅进入 (IN)", static_cast<int>(PassageDirection::ENTRY));
    combo_direction_->addItem("🔴 仅离开 (OUT)", static_cast<int>(PassageDirection::EXIT));
    filter_layout->addWidget(combo_direction_);

    // 客群性别筛选
    auto* lbl_gender = new QLabel("性别:", filter_card);
    lbl_gender->setStyleSheet("font-weight: bold; color: #c9d1d9;");
    filter_layout->addWidget(lbl_gender);

    combo_gender_ = new QComboBox(filter_card);
    combo_gender_->setFixedWidth(75);
    combo_gender_->addItems({"全部性别", "男", "女"});
    filter_layout->addWidget(combo_gender_);

    // 操作按钮组
    btn_query_ = new QPushButton("🔍 筛选查询", filter_card);
    btn_query_->setObjectName("btn_accent");
    btn_query_->setFixedHeight(30);
    filter_layout->addWidget(btn_query_);

    btn_reset_ = new QPushButton("🔄 重置", filter_card);
    btn_reset_->setFixedHeight(30);
    filter_layout->addWidget(btn_reset_);

    btn_export_ = new QPushButton("📥 导出报表", filter_card);
    btn_export_->setObjectName("btn_primary");
    btn_export_->setFixedHeight(30);
    filter_layout->addWidget(btn_export_);

    filter_layout->addStretch();
    main_layout->addWidget(filter_card);

    // ==========================================
    // 2. 核心概览指标小卡片 (5 卡片素净极简矩阵)
    // ==========================================
    auto* kpi_layout = new QHBoxLayout();
    kpi_layout->setSpacing(10);

    kpi_layout->addWidget(create_kpi_card("今日总客流量", total_flow_val_, total_flow_sub_));
    kpi_layout->addWidget(create_kpi_card("进站/入场量", entry_val_, entry_sub_));
    kpi_layout->addWidget(create_kpi_card("出站/离场量", exit_val_, exit_sub_));
    kpi_layout->addWidget(create_kpi_card("客群性别画像", gender_val_, gender_sub_));
    kpi_layout->addWidget(create_kpi_card("客流高峰时段", peak_hour_val_, peak_hour_sub_));

    main_layout->addLayout(kpi_layout);

    // ==========================================
    // 3. 时序走势图表选项卡 (Tab 容器)
    // ==========================================
    tab_charts_ = new QTabWidget(container);
    tab_charts_->setMinimumHeight(410);

    // Tab 1: 实时的总计人数动态 (默认激活 Tab)
    realtime_total_chart_ = new RealtimeTotalChartWidget(tab_charts_);
    tab_charts_->addTab(realtime_total_chart_, "📈  实时总计走势 (动态时序)");

    // Tab 2: 平均人数动态 (5s / 10s / 30s)
    realtime_avg_chart_ = new RealtimeAverageChartWidget(tab_charts_);
    tab_charts_->addTab(realtime_avg_chart_, "⏱️  平均人数走势 (5s/10s/30s)");

    // Tab 3: 24小时时段分布
    hourly_chart_ = new AnalyticsChartWidget(tab_charts_);
    tab_charts_->addTab(hourly_chart_, "📊  24小时各时段进出分布");

    tab_charts_->setCurrentIndex(0); // 默认选定第 1 个 Tab (实时总计人数动态)
    main_layout->addWidget(tab_charts_, 1);

    scroll_area->setWidget(container);
    root_layout->addWidget(scroll_area);

    // 信号槽连接
    connect(combo_preset_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AnalyticsWidget::on_preset_changed);
    connect(btn_query_, &QPushButton::clicked, this, &AnalyticsWidget::on_btn_query_clicked);
    connect(btn_reset_, &QPushButton::clicked, this, &AnalyticsWidget::on_btn_reset_clicked);
    connect(btn_export_, &QPushButton::clicked, this, &AnalyticsWidget::on_btn_export_csv);

    // 默认加载一次统计数据
    refresh_report();
}

QWidget* AnalyticsWidget::create_kpi_card(const QString& title, QLabel*& out_val, QLabel*& out_sub) {
    auto* box = new QWidget();
    box->setObjectName("kpi_card");
    box->setStyleSheet(
        "QWidget#kpi_card {"
        "  background-color: #161b22;"
        "  border: 1px solid #282c34;"
        "  border-radius: 6px;"
        "}"
    );

    auto* v = new QVBoxLayout(box);
    v->setContentsMargins(14, 12, 14, 12);
    v->setSpacing(4);

    auto* t_lbl = new QLabel(title, box);
    t_lbl->setStyleSheet("color: #8c96a3; font-size: 12px; font-weight: 500; border: none; background: transparent;");

    out_val = new QLabel("0 人次", box);
    out_val->setStyleSheet("color: #f0f6fc; font-size: 24px; font-weight: 700; font-family: 'Segoe UI', sans-serif; border: none; background: transparent; margin: 2px 0;");

    out_sub = new QLabel("--", box);
    out_sub->setStyleSheet("color: #6e7681; font-size: 11px; border: none; background: transparent;");

    v->addWidget(t_lbl);
    v->addWidget(out_val);
    v->addWidget(out_sub);
    return box;
}

void AnalyticsWidget::on_preset_changed(int index) {
    QDateTime now = QDateTime::currentDateTime();
    dt_end_->setDateTime(now);

    if (index == 0) {
        // 今日客流汇总 (今日 00:00:00 至今)
        dt_start_->setDateTime(QDateTime(now.date(), QTime(0, 0, 0)));
        is_live_today_view_ = (combo_direction_->currentIndex() == 0 && combo_gender_->currentIndex() == 0);
    } else if (index == 1) {
        // 最近 3 天趋势
        dt_start_->setDateTime(now.addDays(-3));
        is_live_today_view_ = false;
    } else if (index == 2) {
        // 最近 7 天报表
        dt_start_->setDateTime(now.addDays(-7));
        is_live_today_view_ = false;
    } else {
        // 自定义时段
        is_live_today_view_ = false;
    }

    refresh_report();
}

void AnalyticsWidget::on_btn_query_clicked() {
    is_live_today_view_ = false;
    refresh_report();
}

void AnalyticsWidget::on_btn_reset_clicked() {
    combo_preset_->setCurrentIndex(0);
    combo_direction_->setCurrentIndex(0);
    combo_gender_->setCurrentIndex(0);
    QDateTime now = QDateTime::currentDateTime();
    dt_start_->setDateTime(QDateTime(now.date(), QTime(0, 0, 0)));
    dt_end_->setDateTime(now);
    is_live_today_view_ = true;
    refresh_report();
}

void AnalyticsWidget::refresh_report() {
    AnalyticsFilterCriteria criteria;
    criteria.start_ms = dt_start_->dateTime().toMSecsSinceEpoch();
    criteria.end_ms = dt_end_->dateTime().toMSecsSinceEpoch();
    criteria.direction_filter = combo_direction_->currentData().toInt();

    QString sel_gender = combo_gender_->currentText();
    if (sel_gender != "全部性别") {
        criteria.gender_filter = sel_gender;
    }

    // 调用底层多维综合聚合查询
    auto summary = DatabaseManager::instance().query_analytics_summary(criteria);

    // 1. 卡片1: 总客流
    total_flow_val_->setText(QString("%1 人次").arg(summary.total_flow));
    int net = summary.entry_count - summary.exit_count;
    total_flow_sub_->setText(QString("进: %1 │ 出: %2 │ 净增: %3%4")
        .arg(summary.entry_count)
        .arg(summary.exit_count)
        .arg(net > 0 ? "+" : "")
        .arg(net));

    // 2. 卡片2: 进入人次
    entry_val_->setText(QString("%1 人次").arg(summary.entry_count));
    if (summary.total_flow > 0) {
        int entry_pct = (summary.entry_count * 100) / summary.total_flow;
        entry_sub_->setText(QString("占比: %1%").arg(entry_pct));
    } else {
        entry_sub_->setText("所选范围无进入");
    }

    // 3. 卡片3: 离开人次
    exit_val_->setText(QString("%1 人次").arg(summary.exit_count));
    if (summary.total_flow > 0) {
        int exit_pct = (summary.exit_count * 100) / summary.total_flow;
        exit_sub_->setText(QString("占比: %1%").arg(exit_pct));
    } else {
        exit_sub_->setText("所选范围无离开");
    }

    // 4. 卡片4: 客群性别比例
    int valid_gender = summary.male_count + summary.female_count;
    if (valid_gender > 0) {
        int m_pct = (summary.male_count * 100) / valid_gender;
        int f_pct = 100 - m_pct;
        gender_val_->setText(QString("男 %1% │ 女 %2%").arg(m_pct).arg(f_pct));
        gender_sub_->setText(QString("男: %1人 │ 女: %2人").arg(summary.male_count).arg(summary.female_count));
    } else {
        gender_val_->setText("暂无数据");
        gender_sub_->setText("有效识别样本: 0 人");
    }

    // 5. 卡片5: 客流高峰时段
    if (summary.peak_hour >= 0 && summary.peak_hour_count > 0) {
        peak_hour_val_->setText(QString("%1:00 ~ %2:00")
            .arg(summary.peak_hour, 2, 10, QChar('0'))
            .arg((summary.peak_hour + 1) % 24, 2, 10, QChar('0')));
        peak_hour_sub_->setText(QString("小时峰值: %1 人次").arg(summary.peak_hour_count));
    } else {
        peak_hour_val_->setText("-- : --");
        peak_hour_sub_->setText("客流平缓或无数据");
    }

    // 同步刷新 24 小时各时段分布图表
    if (hourly_chart_) {
        hourly_chart_->set_data(summary.hourly_data);
    }

    // 同步初始化/更新实时走势图表的基准数值
    if (realtime_total_chart_) {
        int today_entry = 0, today_exit = 0;
        DatabaseManager::instance().get_today_counts(today_entry, today_exit);
        realtime_total_chart_->set_counts(today_entry + today_exit, today_entry, today_exit);
    }
}

void AnalyticsWidget::update_realtime_stats(const StatisticsSnapshot& stats) {
    if (realtime_total_chart_) {
        realtime_total_chart_->set_counts(stats.today_total, stats.today_entry, stats.today_exit);
    }

    if (is_live_today_view_) {
        total_flow_val_->setText(QString("%1 人次").arg(stats.today_total));
        int net = stats.today_entry - stats.today_exit;
        total_flow_sub_->setText(QString("进: %1 │ 出: %2 │ 净增: %3%4")
            .arg(stats.today_entry).arg(stats.today_exit).arg(net > 0 ? "+" : "").arg(net));

        entry_val_->setText(QString("%1 人次").arg(stats.today_entry));
        exit_val_->setText(QString("%1 人次").arg(stats.today_exit));

        if (stats.today_total > 0) {
            entry_sub_->setText(QString("占比: %1%").arg((stats.today_entry * 100) / stats.today_total));
            exit_sub_->setText(QString("占比: %1%").arg((stats.today_exit * 100) / stats.today_total));
        }
    }
}

void AnalyticsWidget::add_realtime_passage(const PassageEvent& ev) {
    if (realtime_total_chart_) {
        realtime_total_chart_->add_passage(ev);
    }
    if (realtime_avg_chart_) {
        realtime_avg_chart_->add_passage(ev);
    }
}

void AnalyticsWidget::reset_realtime_data() {
    if (realtime_total_chart_) realtime_total_chart_->reset_data();
    if (realtime_avg_chart_) realtime_avg_chart_->reset_data();
    refresh_report();
}

void AnalyticsWidget::on_btn_export_csv() {
    QDir dir("exports");
    if (!dir.exists()) dir.mkpath(".");

    QString default_filename = QString("exports/report_%1.csv").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    QString filename = QFileDialog::getSaveFileName(this, "导出客流明细 CSV 报表", default_filename, "CSV 表格文件 (*.csv);;所有文件 (*.*)");
    if (filename.isEmpty()) {
        return; // 用户主动取消
    }

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "错误", QString("无法创建或写入 CSV 报表文件:\n%1").arg(filename));
        return;
    }

    QTextStream out(&file);
    out << "EventID,Timestamp,RoiID,RoiName,TrackID,Direction,Confidence,Gender,AgeGroup,ClipPath\n";

    int64_t start_ms = dt_start_->dateTime().toMSecsSinceEpoch();
    int64_t end_ms = dt_end_->dateTime().toMSecsSinceEpoch();
    int dir_filter = combo_direction_->currentData().toInt();
    QString gender_filter = combo_gender_->currentText();
    if (gender_filter == "全部性别") gender_filter = "";

    auto events = DatabaseManager::instance().query_filtered_events(dir_filter, start_ms, end_ms, "", 50000, gender_filter);
    int id = 1;
    for (const auto& ev : events) {
        QString time_str = QDateTime::fromMSecsSinceEpoch(ev.timestamp_ms).toString("yyyy-MM-dd hh:mm:ss");
        QString dir_str = (ev.direction == PassageDirection::ENTRY) ? "ENTRY" : "EXIT";
        QString roi_name_str = QString::fromStdString(ev.roi_name);
        out << id++ << "," << time_str << "," << ev.roi_id << ",\""
            << roi_name_str << "\"," << ev.track_id << "," << dir_str << ","
            << ev.confidence << "," << QString::fromStdString(ev.gender) << ","
            << QString::fromStdString(ev.age_group) << "," << QString::fromStdString(ev.clip_path) << "\n";
    }

    file.close();

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("导出成功");
    msgBox.setText(QString("所选时段筛选客流报表 (共 %1 条) 已成功导出至:\n%2").arg(events.size()).arg(filename));
    auto* btn_open_dir = msgBox.addButton("打开所在文件夹", QMessageBox::ActionRole);
    msgBox.addButton("完成", QMessageBox::AcceptRole);
    msgBox.exec();

    if (msgBox.clickedButton() == btn_open_dir) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(filename).absolutePath()));
    }
}
