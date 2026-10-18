#pragma once
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QLineEdit>
#include "common/Types.hpp"

class VideoCanvas;

class StudioWidget : public QWidget {
    Q_OBJECT
public:
    explicit StudioWidget(QWidget* parent = nullptr);

    void update_frame(const QImage& frame, const std::vector<TrackObject>& tracks);
    void set_roi(const RoiConfig& roi);
    void set_rois(const std::vector<RoiConfig>& rois);

signals:
    void roi_saved(const RoiConfig& roi);
    void rois_saved(const std::vector<RoiConfig>& rois);

private slots:
    void on_btn_start_draw();
    void on_btn_reset();
    void on_btn_apply();
    void on_btn_cancel();
    void on_btn_preset_default();
    void on_polygon_status(bool is_valid, int vertex_count);
    void refresh_roi_combo();

private:
    void setup_ui();

    VideoCanvas* canvas_ = nullptr;

    // 多防区 (Multi-ROI) 管理控件
    QComboBox* roi_combo_ = nullptr;
    QLineEdit* roi_name_edit_ = nullptr;
    QPushButton* btn_add_roi_ = nullptr;
    QPushButton* btn_del_roi_ = nullptr;

    QPushButton* btn_draw_ = nullptr;
    QPushButton* btn_apply_ = nullptr;
    QPushButton* btn_cancel_ = nullptr;
    QPushButton* btn_reset_ = nullptr;
    QPushButton* btn_invert_dir_ = nullptr;
    QPushButton* btn_preset_default_ = nullptr;

    bool invert_direction_ = false;

    QLabel* status_label_ = nullptr;
    QSlider* buffer_slider_ = nullptr;
    QLabel* buffer_val_label_ = nullptr;
    QSlider* confirm_slider_ = nullptr;
    QLabel* confirm_val_label_ = nullptr;
};
