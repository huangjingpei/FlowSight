#include "EvidenceWidget.hpp"
#include "storage/DatabaseManager.hpp"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QDateTime>
#include <QFileInfo>
#include <QUrl>
#include <QMessageBox>

EvidenceWidget::EvidenceWidget(QWidget* parent) : QWidget(parent) {
    setup_ui();
    refresh_events();
}

void EvidenceWidget::setup_ui() {
    auto* main_layout = new QHBoxLayout(this);
    main_layout->setContentsMargins(8, 8, 8, 8);
    main_layout->setSpacing(8);

    // 1. 左侧表格区域
    auto* left_panel = new QWidget(this);
    auto* left_vbox = new QVBoxLayout(left_panel);
    left_vbox->setContentsMargins(0, 0, 0, 0);

    auto* top_bar = new QHBoxLayout();
    btn_refresh_ = new QPushButton("刷新流水", this);
    tip_label_ = new QLabel("提示: 双击任意记录行，右侧播放器将自动秒级起播对应 3 秒短视频切片", this);
    tip_label_->setStyleSheet("color: #aaa; font-size: 12px;");
    top_bar->addWidget(btn_refresh_);
    top_bar->addWidget(tip_label_);
    top_bar->addStretch();
    left_vbox->addLayout(top_bar);

    table_ = new QTableWidget(this);
    table_->setColumnCount(6);
    table_->setHorizontalHeaderLabels({"发生时间", "通行方向", "Track ID", "性别", "年龄段", "验证切片文件"});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setStyleSheet("QTableWidget { background: #222; color: #eee; gridline-color: #444; } "
                           "QHeaderView::section { background: #333; color: #fff; padding: 4px; }");
    left_vbox->addWidget(table_);
    main_layout->addWidget(left_panel, 3);

    // 2. 右侧内置微型证据播放器
    player_container_ = new QWidget(this);
    player_container_->setFixedWidth(380);
    auto* player_vbox = new QVBoxLayout(player_container_);
    player_vbox->setContentsMargins(0, 0, 0, 0);

    auto* p_title = new QLabel("通行事件证据播放器 (3秒短片)", this);
    p_title->setStyleSheet("font-weight: bold; color: #00cec9; padding: 4px;");
    player_vbox->addWidget(p_title);

    player_ = new QMediaPlayer(this);
    video_widget_ = new QVideoWidget(this);
    video_widget_->setStyleSheet("background: #000; border: 1px solid #444;");
    player_->setVideoOutput(video_widget_);
    player_vbox->addWidget(video_widget_, 1);

    auto* btn_replay = new QPushButton("重新播放", this);
    btn_replay->setStyleSheet("padding: 6px;");
    player_vbox->addWidget(btn_replay);

    main_layout->addWidget(player_container_, 2);

    // 连接信号槽
    connect(btn_refresh_, &QPushButton::clicked, this, &EvidenceWidget::refresh_events);
    connect(table_, &QTableWidget::cellDoubleClicked, this, &EvidenceWidget::on_cell_double_clicked);
    connect(btn_replay, &QPushButton::clicked, this, [this]() {
        player_->setPosition(0);
        player_->play();
    });
}

void EvidenceWidget::refresh_events() {
    auto events = DatabaseManager::instance().query_recent_events(50);
    table_->setRowCount(0);

    for (const auto& ev : events) {
        add_new_event(ev);
    }
}

void EvidenceWidget::add_new_event(const PassageEvent& ev) {
    int row = table_->rowCount();
    table_->insertRow(row);

    QString time_str = QDateTime::fromMSecsSinceEpoch(ev.timestamp_ms).toString("yyyy-MM-dd hh:mm:ss");
    QString dir_str = (ev.direction == PassageDirection::ENTRY) ? "进入 [ENTRY]" : "离开 [EXIT]";
    QColor dir_color = (ev.direction == PassageDirection::ENTRY) ? QColor(46, 204, 113) : QColor(231, 76, 60);

    auto* item_time = new QTableWidgetItem(time_str);
    auto* item_dir = new QTableWidgetItem(dir_str);
    item_dir->setForeground(dir_color);
    auto* item_id = new QTableWidgetItem(QString("ID: %1").arg(ev.track_id));
    auto* item_gender = new QTableWidgetItem(QString::fromStdString(ev.gender));
    auto* item_age = new QTableWidgetItem(QString::fromStdString(ev.age_group));
    auto* item_clip = new QTableWidgetItem(QString::fromStdString(ev.clip_path));

    table_->setItem(row, 0, item_time);
    table_->setItem(row, 1, item_dir);
    table_->setItem(row, 2, item_id);
    table_->setItem(row, 3, item_gender);
    table_->setItem(row, 4, item_age);
    table_->setItem(row, 5, item_clip);
}

void EvidenceWidget::on_cell_double_clicked(int row, int) {
    if (row < 0 || row >= table_->rowCount()) return;
    auto* clip_item = table_->item(row, 5);
    if (!clip_item) return;

    QString file_path = clip_item->text();
    play_clip(file_path);
}

void EvidenceWidget::play_clip(const QString& file_path) {
    if (file_path.isEmpty() || !QFileInfo::exists(file_path)) {
        QMessageBox::information(this, "提示", QString("切片文件不存在或正在异步落盘: %1").arg(file_path));
        return;
    }
    player_->setSource(QUrl::fromLocalFile(file_path));
    player_->play();
}
