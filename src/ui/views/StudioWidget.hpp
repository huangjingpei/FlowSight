#pragma once
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include "common/Types.hpp"

class VideoCanvas;

class StudioWidget : public QWidget {
    Q_OBJECT
public:
    explicit StudioWidget(QWidget* parent = nullptr);

    void update_frame(const QImage& frame, const std::vector<TrackObject>& tracks);
    void set_roi(const RoiConfig& roi);

signals:
    void roi_saved(const RoiConfig& roi);

private slots:
    void on_btn_start_draw();
    void on_btn_reset();
    void on_btn_apply();
    void on_btn_cancel();
    void on_polygon_status(bool is_valid, int vertex_count);

private:
    void setup_ui();

    VideoCanvas* canvas_ = nullptr;
    QPushButton* btn_draw_ = nullptr;
    QPushButton* btn_apply_ = nullptr;
    QPushButton* btn_cancel_ = nullptr;
    QPushButton* btn_reset_ = nullptr;

    QLabel* status_label_ = nullptr;
    QSlider* buffer_slider_ = nullptr;
    QLabel* buffer_val_label_ = nullptr;
    QSlider* confirm_slider_ = nullptr;
    QLabel* confirm_val_label_ = nullptr;
};
