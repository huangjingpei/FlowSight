#include "AnalyticsWidget.hpp"
#include "storage/ConfigManager.hpp"
#include "ui/components/RealtimeTotalChartWidget.hpp"
#include "ui/components/RealtimeAverageChartWidget.hpp"
#include "ui/components/AnalyticsChartWidget.hpp"
#include "storage/DatabaseManager.hpp"
#include <QSqlQuery>
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
#include <QRegularExpression>
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
    filter_layout->setContentsMargins(8, 4, 8, 4);
    filter_layout->setSpacing(8);

    const QString lbl_style = "color: #8b949e; font-size: 13px; font-weight: 500; border: none; background: transparent;";

    bool is_veh = ConfigManager::instance().is_vehicle_mode();

    // 时间预设
    auto* lbl_preset = new QLabel("统计时段:", filter_card);
    lbl_preset->setStyleSheet(lbl_style);
    filter_layout->addWidget(lbl_preset);

    combo_preset_ = new QComboBox(filter_card);
    combo_preset_->setFixedWidth(118);
    combo_preset_->setFixedHeight(30);
    combo_preset_->addItems({is_veh ? "今日车流汇总" : "今日客流汇总", "最近 3 天趋势", "最近 7 天报表", "自定义时段"});
    combo_preset_->setToolTip("选择常用快捷时间窗口或自定义检索");
    filter_layout->addWidget(combo_preset_);

    // 时间范围
    auto* lbl_time = new QLabel("时间范围:", filter_card);
    lbl_time->setStyleSheet(lbl_style);
    filter_layout->addWidget(lbl_time);

    QDateTime now = QDateTime::currentDateTime();
    dt_start_ = new QDateTimeEdit(filter_card);
    dt_start_->setDisplayFormat("yyyy-MM-dd HH:mm");
    dt_start_->setCalendarPopup(true);
    dt_start_->setFixedWidth(145);
    dt_start_->setFixedHeight(30);
    dt_start_->setDateTime(QDateTime(now.date(), QTime(0, 0, 0)));
    dt_start_->setToolTip("统计起始时间");
    filter_layout->addWidget(dt_start_);

    auto* lbl_to = new QLabel("至", filter_card);
    lbl_to->setStyleSheet("color: #8b949e; font-size: 13px; font-weight: 500; border: none; background: transparent; margin: 0 1px;");
    filter_layout->addWidget(lbl_to);

    dt_end_ = new QDateTimeEdit(filter_card);
    dt_end_->setDisplayFormat("yyyy-MM-dd HH:mm");
    dt_end_->setCalendarPopup(true);
    dt_end_->setFixedWidth(145);
    dt_end_->setFixedHeight(30);
    dt_end_->setDateTime(now);
    dt_end_->setToolTip("统计截止时间");
    filter_layout->addWidget(dt_end_);

    // 穿行方向筛选
    auto* lbl_dir = new QLabel("方向:", filter_card);
    lbl_dir->setStyleSheet(lbl_style);
    filter_layout->addWidget(lbl_dir);

    combo_direction_ = new QComboBox(filter_card);
    combo_direction_->setFixedWidth(108);
    combo_direction_->setFixedHeight(30);
    combo_direction_->addItem("全部方向", -1);
    combo_direction_->addItem(is_veh ? "🟢 仅进车 (IN)" : "🟢 仅进入 (IN)", static_cast<int>(PassageDirection::ENTRY));
    combo_direction_->addItem(is_veh ? "🔴 仅出车 (OUT)" : "🔴 仅离开 (OUT)", static_cast<int>(PassageDirection::EXIT));
    combo_direction_->setToolTip(is_veh ? "按车辆进出卡口方向过滤" : "按进出通道穿行方向过滤");
    filter_layout->addWidget(combo_direction_);

    // 结构化画像属性筛选
    lbl_gender_ = new QLabel(is_veh ? "车型:" : "性别:", filter_card);
    lbl_gender_->setStyleSheet(lbl_style);
    filter_layout->addWidget(lbl_gender_);

    combo_gender_ = new QComboBox(filter_card);
    combo_gender_->setFixedWidth(95);
    combo_gender_->setFixedHeight(30);
    if (is_veh) {
        combo_gender_->addItems({"全部车型", "轿车", "SUV", "客车", "货车", "摩托车"});
        combo_gender_->setToolTip("按车辆分类特征过滤");
    } else {
        combo_gender_->addItems({"全部性别", "男", "女"});
        combo_gender_->setToolTip("按客群识别性别特征过滤");
    }
    filter_layout->addWidget(combo_gender_);

    // 操作按钮组
    btn_query_ = new QPushButton("🔍 筛选查询", filter_card);
    btn_query_->setObjectName("btn_accent");
    btn_query_->setFixedHeight(30);
    btn_query_->setToolTip("按当前全部设置条件执行检索");
    filter_layout->addWidget(btn_query_);

    btn_reset_ = new QPushButton("🔄 重置", filter_card);
    btn_reset_->setFixedHeight(30);
    btn_reset_->setToolTip(is_veh ? "一键恢复默认今日车流实时统计" : "一键恢复默认今日实时统计");
    filter_layout->addWidget(btn_reset_);

    btn_export_ = new QPushButton("📥 导出报表", filter_card);
    btn_export_->setObjectName("btn_primary");
    btn_export_->setFixedHeight(30);
    btn_export_->setToolTip(is_veh ? "将当前筛选范围内全部车流明细记录导出为标准 CSV 表格" : "将当前筛选范围内全部明细记录导出为标准 CSV 表格");
    filter_layout->addWidget(btn_export_);

    filter_layout->addStretch();
    main_layout->addWidget(filter_card);

    // ==========================================
    // 2. 核心概览指标小卡片 (5 卡片素净极简矩阵)
    // ==========================================
    auto* kpi_layout = new QHBoxLayout();
    kpi_layout->setSpacing(10);

    kpi_layout->addWidget(create_kpi_card(is_veh ? "今日总车流量" : "今日总客流量", total_flow_val_, total_flow_sub_, card_title_total_));
    kpi_layout->addWidget(create_kpi_card(is_veh ? "进车/驶入量" : "进站/入场量", entry_val_, entry_sub_, card_title_entry_));
    kpi_layout->addWidget(create_kpi_card(is_veh ? "出车/驶出量" : "出站/离场量", exit_val_, exit_sub_, card_title_exit_));
    kpi_layout->addWidget(create_kpi_card(is_veh ? "车型结构画像" : "客群性别画像", gender_val_, gender_sub_, card_title_gender_));
    kpi_layout->addWidget(create_kpi_card(is_veh ? "车流高峰时段" : "客流高峰时段", peak_hour_val_, peak_hour_sub_, card_title_peak_));

    main_layout->addLayout(kpi_layout);

    // ==========================================
    // 3. 时序走势图表选项卡 (Tab 容器)
    // ==========================================
    tab_charts_ = new QTabWidget(container);
    tab_charts_->setMinimumHeight(410);

    // Tab 1: 实时总计走势 (默认激活 Tab)
    realtime_total_chart_ = new RealtimeTotalChartWidget(tab_charts_);
    tab_charts_->addTab(realtime_total_chart_, is_veh ? "📈  实时总车流走势 (动态时序)" : "📈  实时总计走势 (动态时序)");

    // Tab 2: 平均通量动态 (5s / 10s / 30s)
    realtime_avg_chart_ = new RealtimeAverageChartWidget(tab_charts_);
    tab_charts_->addTab(realtime_avg_chart_, is_veh ? "⏱️  平均车流动态 (5s/10s/30s)" : "⏱️  平均人数走势 (5s/10s/30s)");

    // Tab 3: 24小时时段分布
    hourly_chart_ = new AnalyticsChartWidget(tab_charts_);
    tab_charts_->addTab(hourly_chart_, is_veh ? "📊  24小时车流进出分布" : "📊  24小时各时段进出分布");

    tab_charts_->setCurrentIndex(0); // 默认选定第 1 个 Tab (实时动态时序)
    main_layout->addWidget(tab_charts_, 1);

    scroll_area->setWidget(container);
    root_layout->addWidget(scroll_area);

    // 信号槽连接
    // 1. 预设时段下拉切换
    connect(combo_preset_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AnalyticsWidget::on_preset_changed);

    // 2. 时间控件手动改动时，自动联动切换预设为“自定义时段”并实时过滤
    connect(dt_start_, &QDateTimeEdit::dateTimeChanged, this, &AnalyticsWidget::on_datetime_changed);
    connect(dt_end_, &QDateTimeEdit::dateTimeChanged, this, &AnalyticsWidget::on_datetime_changed);

    // 3. 方向与性别下拉切换即查 (即选即显，零等待操作体验)
    connect(combo_direction_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AnalyticsWidget::on_filter_combo_changed);
    connect(combo_gender_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AnalyticsWidget::on_filter_combo_changed);

    // 4. 操作按钮组
    connect(btn_query_, &QPushButton::clicked, this, &AnalyticsWidget::on_btn_query_clicked);
    connect(btn_reset_, &QPushButton::clicked, this, &AnalyticsWidget::on_btn_reset_clicked);
    connect(btn_export_, &QPushButton::clicked, this, &AnalyticsWidget::on_btn_export_csv);

    // 默认加载一次统计数据
    refresh_report();
}

QWidget* AnalyticsWidget::create_kpi_card(const QString& title, QLabel*& out_val, QLabel*& out_sub, QLabel*& out_title) {
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

    out_title = new QLabel(title, box);
    out_title->setStyleSheet("color: #8c96a3; font-size: 12px; font-weight: 500; border: none; background: transparent;");

    bool is_veh = ConfigManager::instance().is_vehicle_mode();
    out_val = new QLabel(is_veh ? "0 车次" : "0 人次", box);
    out_val->setStyleSheet("color: #f0f6fc; font-size: 24px; font-weight: 700; font-family: 'Segoe UI', sans-serif; border: none; background: transparent; margin: 2px 0;");

    out_sub = new QLabel("--", box);
    out_sub->setStyleSheet("color: #6e7681; font-size: 11px; border: none; background: transparent;");

    v->addWidget(out_title);
    v->addWidget(out_val);
    v->addWidget(out_sub);
    return box;
}

void AnalyticsWidget::on_preset_changed(int index) {
    if (is_updating_preset_) return;
    is_updating_preset_ = true;

    QDateTime now = QDateTime::currentDateTime();
    dt_end_->setDateTime(now);

    if (index == 0) {
        // 今日客流汇总 (今日 00:00:00 至今)
        dt_start_->setDateTime(QDateTime(now.date(), QTime(0, 0, 0)));
        is_live_today_view_ = (combo_direction_->currentIndex() == 0 && combo_gender_->currentIndex() == 0);
        if (tab_charts_ && tab_charts_->currentIndex() != 0) {
            tab_charts_->setCurrentIndex(0); // 恢复激活默认 Tab 1 实时总计走势
        }
    } else if (index == 1) {
        // 最近 3 天趋势 (3 天前 00:00:00 至今)
        dt_start_->setDateTime(QDateTime(now.date().addDays(-3), QTime(0, 0, 0)));
        is_live_today_view_ = false;
        if (tab_charts_ && tab_charts_->currentIndex() != 2) {
            tab_charts_->setCurrentIndex(2); // 智能切至 24小时时段分布走势
        }
    } else if (index == 2) {
        // 最近 7 天报表 (7 天前 00:00:00 至今)
        dt_start_->setDateTime(QDateTime(now.date().addDays(-7), QTime(0, 0, 0)));
        is_live_today_view_ = false;
        if (tab_charts_ && tab_charts_->currentIndex() != 2) {
            tab_charts_->setCurrentIndex(2); // 智能切至 24小时时段分布走势
        }
    } else {
        // 自定义时段
        is_live_today_view_ = false;
    }

    is_updating_preset_ = false;
    refresh_report();
}

void AnalyticsWidget::on_datetime_changed(const QDateTime& /*dt*/) {
    if (is_updating_preset_) return;

    // 当用户手动调整起止时间时，将预设下拉智能切换为“自定义时段” (index 3)
    if (combo_preset_->currentIndex() != 3) {
        is_updating_preset_ = true;
        combo_preset_->setCurrentIndex(3);
        is_updating_preset_ = false;
    }

    // 智能校准起止时间
    if (dt_start_->dateTime() > dt_end_->dateTime()) {
        is_updating_preset_ = true;
        dt_end_->setDateTime(dt_start_->dateTime().addSecs(3600)); // 自动延后 1 小时
        is_updating_preset_ = false;
    }

    is_live_today_view_ = false;
    refresh_report();
}

void AnalyticsWidget::on_filter_combo_changed() {
    is_live_today_view_ = (combo_preset_->currentIndex() == 0 &&
                           combo_direction_->currentIndex() == 0 &&
                           combo_gender_->currentIndex() == 0);
    refresh_report();
}

void AnalyticsWidget::on_btn_query_clicked() {
    if (dt_start_->dateTime() > dt_end_->dateTime()) {
        QMessageBox::warning(this, "时间设置错误", "统计起始时间不得晚于结束时间，请调整后再查询。");
        return;
    }
    is_live_today_view_ = (combo_preset_->currentIndex() == 0 &&
                           combo_direction_->currentIndex() == 0 &&
                           combo_gender_->currentIndex() == 0);
    refresh_report();
}

void AnalyticsWidget::on_btn_reset_clicked() {
    is_updating_preset_ = true;

    // 阻断联动信号，避免重置过程触发多次冗余聚合查询
    QSignalBlocker b1(combo_preset_);
    QSignalBlocker b2(dt_start_);
    QSignalBlocker b3(dt_end_);
    QSignalBlocker b4(combo_direction_);
    QSignalBlocker b5(combo_gender_);

    combo_preset_->setCurrentIndex(0);
    combo_direction_->setCurrentIndex(0);
    combo_gender_->setCurrentIndex(0);

    QDateTime now = QDateTime::currentDateTime();
    dt_start_->setDateTime(QDateTime(now.date(), QTime(0, 0, 0)));
    dt_end_->setDateTime(now);

    is_live_today_view_ = true;
    is_updating_preset_ = false;

    if (tab_charts_) {
        tab_charts_->setCurrentIndex(0); // 恢复激活默认 Tab 1 实时总计走势
    }

    refresh_report();
}

void AnalyticsWidget::refresh_report() {
    bool is_veh = ConfigManager::instance().is_vehicle_mode();
    const QString unit = is_veh ? "车次" : "人次";
    const QString in_str = is_veh ? "进车" : "进";
    const QString out_str = is_veh ? "出车" : "出";

    AnalyticsFilterCriteria criteria;
    criteria.start_ms = dt_start_->dateTime().toMSecsSinceEpoch();
    criteria.end_ms = dt_end_->dateTime().toMSecsSinceEpoch();
    criteria.direction_filter = combo_direction_->currentData().toInt();

    if (!is_veh) {
        QString sel_gender = combo_gender_->currentText();
        if (sel_gender != "全部性别") {
            criteria.gender_filter = sel_gender;
        }
    }

    // 调用底层多维综合聚合查询
    auto summary = DatabaseManager::instance().query_analytics_summary(criteria);

    // 1. 卡片1: 总流量
    total_flow_val_->setText(QString("%1 %2").arg(summary.total_flow).arg(unit));
    int net = summary.entry_count - summary.exit_count;
    total_flow_sub_->setText(QString("%1: %2 │ %3: %4 │ 净增: %5%6")
        .arg(in_str)
        .arg(summary.entry_count)
        .arg(out_str)
        .arg(summary.exit_count)
        .arg(net > 0 ? "+" : "")
        .arg(net));

    // 2. 卡片2: 进入量
    entry_val_->setText(QString("%1 %2").arg(summary.entry_count).arg(unit));
    if (summary.total_flow > 0) {
        int entry_pct = (summary.entry_count * 100) / summary.total_flow;
        entry_sub_->setText(QString("占比: %1%").arg(entry_pct));
    } else {
        entry_sub_->setText(is_veh ? "所选范围无进车" : "所选范围无进入");
    }

    // 3. 卡片3: 离开量
    exit_val_->setText(QString("%1 %2").arg(summary.exit_count).arg(unit));
    if (summary.total_flow > 0) {
        int exit_pct = (summary.exit_count * 100) / summary.total_flow;
        exit_sub_->setText(QString("占比: %1%").arg(exit_pct));
    } else {
        entry_sub_->setText(is_veh ? "所选范围无出车" : "所选范围无离开");
    }

    // 4. 卡片4: 结构化画像 (车流展示车型分布，客流展示性别比例)
    if (is_veh) {
        QSqlDatabase db = DatabaseManager::instance().get_database();
        QSqlQuery q(db);
        q.prepare("SELECT vehicle_type, COUNT(*) FROM passage_events WHERE timestamp_ms >= ? AND timestamp_ms <= ? AND vehicle_type != '' GROUP BY vehicle_type ORDER BY COUNT(*) DESC LIMIT 2;");
        q.addBindValue(static_cast<qint64>(criteria.start_ms));
        q.addBindValue(static_cast<qint64>(criteria.end_ms));
        QString top_types = "";
        int total_sample = 0;
        if (q.exec()) {
            while (q.next()) {
                QString t = q.value(0).toString().trimmed();
                if (t.isEmpty()) t = "轿车";
                int c = q.value(1).toInt();
                total_sample += c;
                if (!top_types.isEmpty()) top_types += " │ ";
                top_types += QString("%1 %2辆").arg(t).arg(c);
            }
        }
        if (!top_types.isEmpty()) {
            gender_val_->setText(top_types);
            gender_sub_->setText(QString("有效车型样本: %1 辆").arg(total_sample));
        } else {
            gender_val_->setText("暂无车型数据");
            gender_sub_->setText("有效识别样本: 0 辆");
        }
    } else {
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
    }

    // 5. 卡片5: 高峰时段
    if (summary.peak_hour >= 0 && summary.peak_hour_count > 0) {
        peak_hour_val_->setText(QString("%1:00 ~ %2:00")
            .arg(summary.peak_hour, 2, 10, QChar('0'))
            .arg((summary.peak_hour + 1) % 24, 2, 10, QChar('0')));
        peak_hour_sub_->setText(QString("小时峰值: %1 %2").arg(summary.peak_hour_count).arg(unit));
    } else {
        peak_hour_val_->setText("-- : --");
        peak_hour_sub_->setText(is_veh ? "车流平缓或无数据" : "客流平缓或无数据");
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
    bool is_veh = ConfigManager::instance().is_vehicle_mode();
    const QString unit = is_veh ? "车次" : "人次";
    const QString in_str = is_veh ? "进车" : "进";
    const QString out_str = is_veh ? "出车" : "出";

    if (realtime_total_chart_) {
        realtime_total_chart_->set_counts(stats.today_total, stats.today_entry, stats.today_exit);
    }

    if (is_live_today_view_) {
        total_flow_val_->setText(QString("%1 %2").arg(stats.today_total).arg(unit));
        int net = stats.today_entry - stats.today_exit;
        total_flow_sub_->setText(QString("%1: %2 │ %3: %4 │ 净增: %5%6")
            .arg(in_str).arg(stats.today_entry).arg(out_str).arg(stats.today_exit).arg(net > 0 ? "+" : "").arg(net));

        entry_val_->setText(QString("%1 %2").arg(stats.today_entry).arg(unit));
        exit_val_->setText(QString("%1 %2").arg(stats.today_exit).arg(unit));

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
    bool is_veh = ConfigManager::instance().is_vehicle_mode();
    int64_t start_ms = dt_start_->dateTime().toMSecsSinceEpoch();
    int64_t end_ms = dt_end_->dateTime().toMSecsSinceEpoch();
    int dir_filter = combo_direction_->currentData().toInt();
    QString gender_filter = "";
    QString keyword_filter = "";

    if (!is_veh) {
        gender_filter = combo_gender_->currentText();
        if (gender_filter == "全部性别") gender_filter = "";
    } else {
        QString veh_type = combo_gender_->currentText();
        if (veh_type != "全部车型") keyword_filter = veh_type;
    }

    auto events = DatabaseManager::instance().query_filtered_events(dir_filter, start_ms, end_ms, keyword_filter, 50000, gender_filter);

    if (events.empty()) {
        auto reply = QMessageBox::question(
            this, "导出提示",
            is_veh ? "当前筛选时段与过滤条件下没有检索到车辆通行明细记录。\n是否仍要导出一份空表？"
                   : "当前筛选时段与过滤条件下没有检索到客流通行明细记录。\n是否仍要导出一份空表？",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No
        );
        if (reply != QMessageBox::Yes) {
            return;
        }
    }

    QDir dir("exports");
    if (!dir.exists()) dir.mkpath(".");

    QString preset_clean = combo_preset_->currentText().remove(QRegularExpression("[^a-zA-Z0-9\\x4e00-\\x9fa5]"));
    QString default_filename = QString("exports/%1_%2_%3.csv")
        .arg(is_veh ? "车流采集明细" : "客流采集明细")
        .arg(preset_clean.isEmpty() ? (is_veh ? "车流统计" : "客流统计") : preset_clean)
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));

    QString filename = QFileDialog::getSaveFileName(
        this,
        is_veh ? "导出车流采集明细 CSV 报表" : "导出客流采集明细 CSV 报表",
        default_filename,
        "CSV 表格文件 (*.csv);;所有文件 (*.*)"
    );
    if (filename.isEmpty()) {
        return; // 用户主动取消
    }

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "导出错误", QString("无法创建或写入 CSV 报表文件:\n%1").arg(filename));
        return;
    }

    QTextStream out(&file);
    // 写入 UTF-8 BOM，彻底解决 Windows Excel 打开中文显示为乱码的问题
    out.setGenerateByteOrderMark(true);

    if (is_veh) {
        out << "序号,通行时间,监测卡口编号,卡口名称,车辆跟踪ID,通行方向,置信度,车型分类,车身颜色,车牌号码,号牌颜色,抓拍切片本地路径\n";
        int id = 1;
        for (const auto& ev : events) {
            QString time_str = QDateTime::fromMSecsSinceEpoch(ev.timestamp_ms).toString("yyyy-MM-dd HH:mm:ss");
            QString dir_str = (ev.direction == PassageDirection::ENTRY) ? "进车 (IN)" : "出车 (OUT)";
            QString roi_name_str = QString::fromStdString(ev.roi_name.empty() ? "卡口通道" : ev.roi_name);
            QString vtype = QString::fromStdString(ev.vehicle_type.empty() ? "轿车" : ev.vehicle_type);
            QString vcolor = QString::fromStdString(ev.vehicle_color.empty() ? "白色" : ev.vehicle_color);
            QString pnum = QString::fromStdString(ev.plate_number.empty() ? "未识别" : ev.plate_number);
            QString pcolor = QString::fromStdString(ev.plate_color.empty() ? "蓝牌" : ev.plate_color);

            out << id++ << ","
                << time_str << ","
                << ev.roi_id << ",\""
                << roi_name_str << "\","
                << ev.track_id << ","
                << dir_str << ","
                << QString::number(ev.confidence, 'f', 2) << ",\""
                << vtype << "\",\""
                << vcolor << "\",\""
                << pnum << "\",\""
                << pcolor << "\",\""
                << QString::fromStdString(ev.clip_path) << "\"\n";
        }
    } else {
        out << "序号,通行时间,检测通道编号,通道名称,行人跟踪ID,通行方向,置信度,性别画像,年龄段画像,抓拍切片本地路径\n";
        int id = 1;
        for (const auto& ev : events) {
            QString time_str = QDateTime::fromMSecsSinceEpoch(ev.timestamp_ms).toString("yyyy-MM-dd HH:mm:ss");
            QString dir_str = (ev.direction == PassageDirection::ENTRY) ? "进入 (IN)" : "离开 (OUT)";
            QString roi_name_str = QString::fromStdString(ev.roi_name.empty() ? "A门主通道" : ev.roi_name);
            QString gender = QString::fromStdString(ev.gender.empty() ? "未知" : ev.gender);
            QString age = QString::fromStdString(ev.age_group.empty() ? "未知" : ev.age_group);

            out << id++ << ","
                << time_str << ","
                << ev.roi_id << ",\""
                << roi_name_str << "\","
                << ev.track_id << ","
                << dir_str << ","
                << QString::number(ev.confidence, 'f', 2) << ",\""
                << gender << "\",\""
                << age << "\",\""
                << QString::fromStdString(ev.clip_path) << "\"\n";
        }
    }

    file.close();

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("导出成功");
    msgBox.setText(QString("%1 (共 %2 条记录) 已成功导出至:\n%3")
        .arg(is_veh ? "车流明细报表" : "客流明细报表")
        .arg(events.size())
        .arg(filename));
    auto* btn_open_dir = msgBox.addButton("打开所在文件夹", QMessageBox::ActionRole);
    msgBox.addButton("完成", QMessageBox::AcceptRole);
    msgBox.exec();

    if (msgBox.clickedButton() == btn_open_dir) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(filename).absolutePath()));
    }
}
