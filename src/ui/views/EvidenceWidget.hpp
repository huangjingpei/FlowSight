#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include "common/Types.hpp"

class MediaPlayerWidget;

class EvidenceWidget : public QWidget {
    Q_OBJECT
public:
    explicit EvidenceWidget(QWidget* parent = nullptr);

    void refresh_events();
    void add_new_event(const PassageEvent& ev);

private slots:
    void on_cell_double_clicked(int row, int column);
    void on_table_selection_changed();
    void play_clip(const QString& file_path);
    void reset_filters();

private:
    void setup_ui();
    bool event_matches_filter(const PassageEvent& ev) const;
    void update_metadata_inspector(int row);
    void populate_table_row(int row, const PassageEvent& ev);

    // 过滤与搜索控件
    QComboBox* combo_direction_ = nullptr;
    QComboBox* combo_timerange_ = nullptr;
    QLineEdit* search_input_ = nullptr;
    QPushButton* btn_refresh_ = nullptr;
    QPushButton* btn_reset_filter_ = nullptr;
    QPushButton* btn_open_local_ = nullptr;
    QLabel* tip_label_ = nullptr;
    QLabel* count_badge_ = nullptr;

    // 表格
    QTableWidget* table_ = nullptr;

    // 右侧溯源回放与元数据检查器
    MediaPlayerWidget* player_ = nullptr;
    QWidget* player_container_ = nullptr;
    QLabel* inspector_time_ = nullptr;
    QLabel* inspector_dir_ = nullptr;
    QLabel* inspector_track_ = nullptr;
    QLabel* inspector_attr_ = nullptr;
    QLabel* inspector_file_ = nullptr;
    QLabel* inspector_status_ = nullptr;
    QPushButton* btn_inspect_play_ = nullptr;

    QString selected_clip_path_;
};

