#include "MonitorWidget.hpp"
#include "ui/VideoCanvas.hpp"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QDateTime>

MonitorWidget::MonitorWidget(QWidget* parent) : QWidget(parent) {
    setup_ui();
}

void MonitorWidget::setup_ui() {
    auto* main_layout = new QHBoxLayout(this);
    main_layout->setContentsMargins(8, 8, 8, 8);
    main_layout->setSpacing(8);

    // 1. 左侧大视频监控区 (只读模式，纯展示，防误触)
    auto* video_container = new QWidget(this);
    auto* video_vbox = new QVBoxLayout(video_container);
    video_vbox->setContentsMargins(0, 0, 0, 0);

    canvas_ = new VideoCanvas(this);
    canvas_->set_read_only(true);
    video_vbox->addWidget(canvas_, 1);

    // 底部跑马灯最新通行动态
    ticker_list_ = new QListWidget(this);
    ticker_list_->setFixedHeight(60);
    ticker_list_->setFlow(QListView::LeftToRight);
    ticker_list_->setWrapping(false);
    ticker_list_->setStyleSheet("QListWidget { background: #222; color: #fff; font-size: 13px; border: 1px solid #444; }");
    video_vbox->addWidget(ticker_list_);

    main_layout->addWidget(video_container, 4);

    // 2. 右侧核心指标看板
    auto* right_panel = new QWidget(this);
    right_panel->setFixedWidth(260);
    auto* right_vbox = new QVBoxLayout(right_panel);
    right_vbox->setContentsMargins(0, 0, 0, 0);

    auto* kpi_group = new QGroupBox("实时客流监控", this);
    auto* kpi_layout = new QVBoxLayout(kpi_group);

    auto create_card = [](const QString& title, const QString& init_val, const QString& color) -> std::pair<QWidget*, QLabel*> {
        auto* card = new QWidget();
        card->setStyleSheet(QString("background: #2a2a2a; border-radius: 6px; padding: 10px; border-left: 4px solid %1;").arg(color));
        auto* vbox = new QVBoxLayout(card);
        vbox->setContentsMargins(5, 5, 5, 5);
        auto* t_lbl = new QLabel(title);
        t_lbl->setStyleSheet("color: #aaa; font-size: 13px;");
        auto* v_lbl = new QLabel(init_val);
        v_lbl->setStyleSheet("color: #fff; font-size: 28px; font-weight: bold;");
        vbox->addWidget(t_lbl);
        vbox->addWidget(v_lbl);
        return {card, v_lbl};
    };

    auto [c1, l1] = create_card("当前区域人数", "0", "#00d2d3");
    occupancy_label_ = l1;
    kpi_layout->addWidget(c1);

    auto [c2, l2] = create_card("今日进入人次", "0", "#10ac84");
    entry_label_ = l2;
    kpi_layout->addWidget(c2);

    auto [c3, l3] = create_card("今日离开人次", "0", "#ee5253");
    exit_label_ = l3;
    kpi_layout->addWidget(c3);

    auto [c4, l4] = create_card("通行总计", "0", "#ff9f43");
    total_label_ = l4;
    kpi_layout->addWidget(c4);

    kpi_layout->addStretch();
    right_vbox->addWidget(kpi_group);

    main_layout->addWidget(right_panel, 1);
}

void MonitorWidget::update_frame(const QImage& frame, const std::vector<TrackObject>& tracks) {
    if (canvas_) {
        canvas_->update_frame(frame, tracks);
    }
}

void MonitorWidget::update_stats(const StatisticsSnapshot& stats) {
    if (occupancy_label_) occupancy_label_->setText(QString::number(stats.current_occupancy));
    if (entry_label_) entry_label_->setText(QString::number(stats.today_entry));
    if (exit_label_) exit_label_->setText(QString::number(stats.today_exit));
    if (total_label_) total_label_->setText(QString::number(stats.today_total));
}

void MonitorWidget::add_passage_event(const PassageEvent& ev) {
    if (!ticker_list_) return;
    QString dir_str = (ev.direction == PassageDirection::ENTRY) ? "进入 [IN]" : "离开 [OUT]";
    QString time_str = QDateTime::fromMSecsSinceEpoch(ev.timestamp_ms).toString("hh:mm:ss");
    QString item_text = QString("[%1] %2 ID:%3 (%4)").arg(time_str).arg(dir_str).arg(ev.track_id).arg(QString::fromStdString(ev.gender));

    ticker_list_->insertItem(0, item_text);
    if (ticker_list_->count() > 10) {
        delete ticker_list_->takeItem(10);
    }
}

void MonitorWidget::update_active_roi(const RoiConfig& roi) {
    if (canvas_) {
        canvas_->set_roi(roi);
    }
}
