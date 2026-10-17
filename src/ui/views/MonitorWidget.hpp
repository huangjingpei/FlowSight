#pragma once
#include <QWidget>
#include <QLabel>
#include <QListWidget>
#include "common/Types.hpp"

class VideoCanvas;

class MonitorWidget : public QWidget {
    Q_OBJECT
public:
    explicit MonitorWidget(QWidget* parent = nullptr);

    void update_frame(const QImage& frame, const std::vector<TrackObject>& tracks);
    void update_stats(const StatisticsSnapshot& stats);
    void add_passage_event(const PassageEvent& ev);
    void update_active_roi(const RoiConfig& roi);

private:
    void setup_ui();

    VideoCanvas* canvas_ = nullptr;
    QLabel* occupancy_label_ = nullptr;
    QLabel* entry_label_ = nullptr;
    QLabel* exit_label_ = nullptr;
    QLabel* total_label_ = nullptr;
    QListWidget* ticker_list_ = nullptr;
};
