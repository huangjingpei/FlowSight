#pragma once
#include <QWidget>
#include <QImage>
#include <QPointF>
#include <vector>
#include <opencv2/core.hpp>
#include "common/Types.hpp"

class VideoCanvas : public QWidget {
    Q_OBJECT
public:
    enum class EditMode { IDLE, DRAWING, EDITING };

    explicit VideoCanvas(QWidget* parent = nullptr);

    void set_read_only(bool read_only);
    void update_frame(const QImage& frame, const std::vector<TrackObject>& tracks);
    void set_roi(const RoiConfig& roi);
    RoiConfig get_roi() const;

    // 多区域 (Multi-ROI) 交互管理接口
    void set_rois(const std::vector<RoiConfig>& rois);
    std::vector<RoiConfig> get_rois() const;
    void set_active_roi_index(int idx);
    int active_roi_index() const { return active_roi_idx_; }
    void add_roi(const RoiConfig& roi);
    void remove_active_roi();
    void set_active_roi_name(const std::string& name);
    void set_active_roi_invert_direction(bool invert);
    void set_active_roi_buffer_dist(float dist);
    void set_active_roi_confirm_frames(int frames);

    void start_drawing();
    void reset_roi();
    void apply_changes();
    void cancel_changes();

    int video_width() const { return static_cast<int>(video_w_); }
    int video_height() const { return static_cast<int>(video_h_); }

    // 客流超载呼吸告警
    void set_crowd_surge_alert(bool active, int current_count = 0, int threshold = 0);
    bool is_crowd_surge_active() const { return crowd_surge_active_; }

signals:
    void roi_applied(const RoiConfig& roi);
    void rois_applied(const std::vector<RoiConfig>& rois);
    void active_roi_changed(int index, const RoiConfig& roi);
    void polygon_status_changed(bool is_valid, int vertex_count);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QPointF frame_to_canvas(const cv::Point2f& pt_f) const;
    cv::Point2f canvas_to_frame(const QPointF& pt_c) const;
    bool is_point_in_video_rect(const QPointF& pt_c) const;
    int get_hovered_vertex_index(const QPointF& pt_c) const;
    bool validate_polygon(const std::vector<cv::Point2f>& pts) const;
    void draw_standby_screen(QPainter& painter);

    bool read_only_ = false;
    QImage current_frame_;
    std::vector<TrackObject> current_tracks_;

    std::vector<RoiConfig> rois_;
    int active_roi_idx_ = 0;
    RoiConfig active_roi_;
    RoiConfig editing_roi_;

    EditMode mode_ = EditMode::IDLE;
    int dragging_vertex_idx_ = -1;
    bool is_dragging_whole_ = false;
    QPointF last_mouse_pos_;
    QPointF current_mouse_hover_;

    // 客流超载赛博呼吸告警状态
    bool crowd_surge_active_ = false;
    int surge_occupancy_ = 0;
    int surge_threshold_ = 0;
    class QTimer* surge_anim_timer_ = nullptr;
    float surge_anim_phase_ = 0.0f;

    // Letterbox 缩放与偏移
    float scale_ = 1.0f;
    float offset_x_ = 0.0f;
    float offset_y_ = 0.0f;
    float video_w_ = 0.0f;
    float video_h_ = 0.0f;
};
