#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QMediaPlayer>
#include <QVideoWidget>
#include "common/Types.hpp"

class EvidenceWidget : public QWidget {
    Q_OBJECT
public:
    explicit EvidenceWidget(QWidget* parent = nullptr);

    void refresh_events();
    void add_new_event(const PassageEvent& ev);

private slots:
    void on_cell_double_clicked(int row, int column);
    void play_clip(const QString& file_path);

private:
    void setup_ui();

    QTableWidget* table_ = nullptr;
    QPushButton* btn_refresh_ = nullptr;
    QLabel* tip_label_ = nullptr;

    QMediaPlayer* player_ = nullptr;
    QVideoWidget* video_widget_ = nullptr;
    QWidget* player_container_ = nullptr;
};
