#include "VideoCanvas.hpp"
#include <QPainter>
#include <QMouseEvent>
#include <QTimer>
#include <cmath>
#include <opencv2/imgproc.hpp>
#include <algorithm>

VideoCanvas::VideoCanvas(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setMinimumSize(480, 270);

    surge_anim_timer_ = new QTimer(this);
    surge_anim_timer_->setInterval(30);
    connect(surge_anim_timer_, &QTimer::timeout, this, [this]() {
        if (crowd_surge_active_) {
            surge_anim_phase_ += 0.12f;
            if (surge_anim_phase_ > 6.2831853f) surge_anim_phase_ -= 6.2831853f;
            update();
        }
    });
}

void VideoCanvas::set_crowd_surge_alert(bool active, int current_count, int threshold) {
    crowd_surge_active_ = active;
    surge_occupancy_ = current_count;
    surge_threshold_ = threshold;
    if (active) {
        if (!surge_anim_timer_->isActive()) {
            surge_anim_timer_->start();
        }
    } else {
        if (surge_anim_timer_->isActive()) {
            surge_anim_timer_->stop();
        }
    }
    update();
}

void VideoCanvas::set_read_only(bool read_only) {
    read_only_ = read_only;
    if (read_only_) {
        mode_ = EditMode::IDLE;
        setCursor(Qt::ArrowCursor);
    }
}

void VideoCanvas::update_frame(const QImage& frame, const std::vector<TrackObject>& tracks) {
    current_frame_ = frame;
    current_tracks_ = tracks;
    update();
}

void VideoCanvas::set_rois(const std::vector<RoiConfig>& rois) {
    rois_ = rois;
    if (rois_.empty()) {
        int w = video_w_ > 0 ? static_cast<int>(video_w_) : 1920;
        int h = video_h_ > 0 ? static_cast<int>(video_h_) : 1080;
        rois_.push_back(make_default_corridor_roi(w, h, 1, "A门主通道", "#58a6ff"));
    }
    if (active_roi_idx_ >= static_cast<int>(rois_.size()) || active_roi_idx_ < 0) {
        active_roi_idx_ = 0;
    }
    active_roi_ = rois_[active_roi_idx_];
    editing_roi_ = active_roi_;
    update();
}

std::vector<RoiConfig> VideoCanvas::get_rois() const {
    return rois_;
}

void VideoCanvas::set_active_roi_index(int idx) {
    if (idx >= 0 && idx < static_cast<int>(rois_.size())) {
        active_roi_idx_ = idx;
        active_roi_ = rois_[active_roi_idx_];
        editing_roi_ = active_roi_;
        update();
        emit active_roi_changed(active_roi_idx_, active_roi_);
        emit polygon_status_changed(editing_roi_.is_valid, static_cast<int>(editing_roi_.vertices.size()));
    }
}

void VideoCanvas::add_roi(const RoiConfig& roi) {
    rois_.push_back(roi);
    active_roi_idx_ = static_cast<int>(rois_.size()) - 1;
    active_roi_ = rois_[active_roi_idx_];
    editing_roi_ = active_roi_;
    update();
    emit active_roi_changed(active_roi_idx_, active_roi_);
    emit rois_applied(rois_);
}

void VideoCanvas::remove_active_roi() {
    if (rois_.size() <= 1) {
        reset_roi();
        return;
    }
    rois_.erase(rois_.begin() + active_roi_idx_);
    if (active_roi_idx_ >= static_cast<int>(rois_.size())) {
        active_roi_idx_ = static_cast<int>(rois_.size()) - 1;
    }
    active_roi_ = rois_[active_roi_idx_];
    editing_roi_ = active_roi_;
    update();
    emit active_roi_changed(active_roi_idx_, active_roi_);
    emit rois_applied(rois_);
}

void VideoCanvas::set_active_roi_name(const std::string& name) {
    if (active_roi_idx_ >= 0 && active_roi_idx_ < static_cast<int>(rois_.size())) {
        rois_[active_roi_idx_].name = name;
        editing_roi_.name = name;
        active_roi_.name = name;
        update();
    }
}

void VideoCanvas::set_active_roi_invert_direction(bool invert) {
    if (active_roi_idx_ >= 0 && active_roi_idx_ < static_cast<int>(rois_.size())) {
        rois_[active_roi_idx_].invert_direction = invert;
        editing_roi_.invert_direction = invert;
        active_roi_.invert_direction = invert;
        update();
    }
}

void VideoCanvas::set_active_roi_buffer_dist(float dist) {
    if (active_roi_idx_ >= 0 && active_roi_idx_ < static_cast<int>(rois_.size())) {
        rois_[active_roi_idx_].buffer_distance = dist;
        editing_roi_.buffer_distance = dist;
        active_roi_.buffer_distance = dist;
        update();
    }
}

void VideoCanvas::set_active_roi_confirm_frames(int frames) {
    if (active_roi_idx_ >= 0 && active_roi_idx_ < static_cast<int>(rois_.size())) {
        rois_[active_roi_idx_].min_confirm_frames = frames;
        editing_roi_.min_confirm_frames = frames;
        active_roi_.min_confirm_frames = frames;
        update();
    }
}

void VideoCanvas::set_roi(const RoiConfig& roi) {
    if (rois_.empty()) {
        rois_.push_back(roi);
        active_roi_idx_ = 0;
    } else if (active_roi_idx_ >= 0 && active_roi_idx_ < static_cast<int>(rois_.size())) {
        rois_[active_roi_idx_] = roi;
    }
    active_roi_ = roi;
    editing_roi_ = roi;
    update();
}

RoiConfig VideoCanvas::get_roi() const {
    if (active_roi_idx_ >= 0 && active_roi_idx_ < static_cast<int>(rois_.size())) {
        return rois_[active_roi_idx_];
    }
    return active_roi_;
}

void VideoCanvas::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.fillRect(rect(), QColor(13, 17, 23));

    if (current_frame_.isNull()) {
        draw_standby_screen(painter);
        return;
    }

    // 1. 等比 Letterbox 映射计算
    float canvas_w = width();
    float canvas_h = height();
    video_w_ = current_frame_.width();
    video_h_ = current_frame_.height();

    scale_ = std::min(canvas_w / video_w_, canvas_h / video_h_);
    float render_w = video_w_ * scale_;
    float render_h = video_h_ * scale_;
    offset_x_ = (canvas_w - render_w) * 0.5f;
    offset_y_ = (canvas_h - render_h) * 0.5f;

    // 2. 绘制视频底图
    QRectF target_rect(offset_x_, offset_y_, render_w, render_h);
    painter.drawImage(target_rect, current_frame_);

    // 3. 绘制多区域 (Multi-ROI) 几何防区与指示
    int total_rois = static_cast<int>(rois_.size());
    bool has_any_valid_roi = false;

    for (int r_idx = 0; r_idx < total_rois; ++r_idx) {
        bool is_active_editing = (mode_ != EditMode::IDLE && r_idx == active_roi_idx_);
        const auto& roi = is_active_editing ? editing_roi_ : rois_[r_idx];
        if (roi.vertices.empty()) continue;

        QPolygonF poly;
        for (const auto& v : roi.vertices) {
            float vx = v.x;
            float vy = v.y;
            if (roi.ref_width > 0 && video_w_ > 0 && roi.ref_width != static_cast<int>(video_w_)) {
                vx = vx * video_w_ / static_cast<float>(roi.ref_width);
            }
            if (roi.ref_height > 0 && video_h_ > 0 && roi.ref_height != static_cast<int>(video_h_)) {
                vy = vy * video_h_ / static_cast<float>(roi.ref_height);
            }
            poly << QPointF(vx * scale_ + offset_x_, vy * scale_ + offset_y_);
        }

        if (poly.isEmpty()) continue;

        QColor base_color(QString::fromStdString(roi.color_hex.empty() ? "#58a6ff" : roi.color_hex));
        if (!base_color.isValid()) base_color = QColor("#58a6ff");

        if (is_active_editing && mode_ == EditMode::DRAWING) {
            painter.setPen(QPen(base_color, 2, Qt::DashLine));
            painter.drawPolyline(poly);
            if (!poly.isEmpty()) {
                painter.drawLine(poly.last(), current_mouse_hover_);
            }
            painter.setBrush(base_color);
            for (const auto& pt : poly) {
                painter.drawEllipse(pt, 5, 5);
            }
            has_any_valid_roi = true;
            continue;
        }

        if (poly.size() >= 3) {
            has_any_valid_roi = true;
            // 填充色：非编辑状态或激活状态柔和半透明
            int alpha_fill = is_active_editing ? 70 : 40;
            QColor fill_col = base_color;
            fill_col.setAlpha(alpha_fill);
            painter.setBrush(fill_col);

            QPen border_pen(base_color, is_active_editing ? 2.5f : 1.8f, is_active_editing ? Qt::SolidLine : Qt::DashLine);
            painter.setPen(border_pen);
            painter.drawPolygon(poly);

            // 绘制防区重心、方向箭头与标识徽标
            QPointF centroid(0, 0);
            for (const auto& pt : poly) centroid += pt;
            centroid /= poly.size();

            // 计算法向方向矢量
            QPointF upper_half(0, 0); int upper_cnt = 0;
            QPointF lower_half(0, 0); int lower_cnt = 0;
            for (const auto& pt : poly) {
                if (pt.y() < centroid.y()) { upper_half += pt; upper_cnt++; }
                else { lower_half += pt; lower_cnt++; }
            }
            QPointF base_dir(0, 1);
            if (upper_cnt > 0 && lower_cnt > 0) {
                base_dir = (lower_half / lower_cnt) - (upper_half / upper_cnt);
            }
            float dir_len = std::hypot(base_dir.x(), base_dir.y());
            if (dir_len > 1e-3f) base_dir /= dir_len;
            else base_dir = QPointF(0, 1);

            if (roi.invert_direction) base_dir = -base_dir;

            float arrow_len = 34.0f;
            QPointF p_start = centroid - base_dir * (arrow_len * 0.45f);
            QPointF p_end = centroid + base_dir * (arrow_len * 0.55f);

            painter.setPen(QPen(base_color, 2.5f, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(p_start, p_end);

            painter.setPen(Qt::NoPen);
            QColor halo = base_color; halo.setAlpha(120);
            painter.setBrush(halo);
            painter.drawEllipse(p_start, 5, 5);
            painter.setBrush(base_color);
            painter.drawEllipse(p_start, 2.5, 2.5);

            QPointF perp(-base_dir.y(), base_dir.x());
            float wing_len = 10.0f; float wing_w = 5.5f;
            QPointF wing1 = p_end - base_dir * wing_len + perp * wing_w;
            QPointF wing2 = p_end - base_dir * wing_len - perp * wing_w;
            QPolygonF head_poly;
            head_poly << p_end << wing1 << (p_end - base_dir * (wing_len * 0.7f)) << wing2;
            painter.setBrush(base_color);
            painter.drawPolygon(head_poly);

            // 绘制防区名称与方向胶囊徽标 [A门主通道 | 进 ➔]
            QString name_str = QString::fromStdString(roi.name.empty() ? QString("防区 %1").arg(roi.roi_id).toStdString() : roi.name);
            QString dir_tag = QString("%1  进➔").arg(name_str);
            QFont dir_font("Segoe UI", 9, QFont::Bold);
            QFontMetrics dfm(dir_font);
            int tag_w = dfm.horizontalAdvance(dir_tag) + 12;
            int tag_h = 19;
            QPointF tag_pos = centroid + perp * 18.0f - QPointF(tag_w / 2.0f, tag_h / 2.0f);
            QRectF tag_rect(tag_pos.x(), tag_pos.y(), tag_w, tag_h);

            painter.setPen(QPen(base_color, 1));
            painter.setBrush(QColor(13, 17, 23, 220));
            painter.drawRoundedRect(tag_rect, 4, 4);

            painter.setFont(dir_font);
            painter.setPen(base_color);
            painter.drawText(tag_rect, Qt::AlignCenter, dir_tag);

            // 标定编辑模式下呈现当前选中防区的可拖拽控制点
            if (is_active_editing && mode_ == EditMode::EDITING && !read_only_) {
                painter.setBrush(Qt::white);
                painter.setPen(QPen(Qt::red, 2));
                for (int i = 0; i < poly.size(); ++i) {
                    painter.drawEllipse(poly[i], 6, 6);
                }
            }
        }
    }

    if (!has_any_valid_roi && mode_ == EditMode::IDLE) {
        // 未标定或清空防区时，呈现全域感知模式提示胶囊
        painter.setFont(QFont("Segoe UI", 10, QFont::Bold));
        QString hint = "💡 全域感知模式 (全画面人数实时统计) │ 前往【📐 空间标定】可绘制进出分流防区";
        QFontMetrics fm(painter.font());
        int hint_w = fm.horizontalAdvance(hint) + 24;
        int hint_h = 28;
        QRectF hint_rect(offset_x_ + (render_w - hint_w) * 0.5f, offset_y_ + 14, hint_w, hint_h);

        painter.setPen(QPen(QColor(88, 166, 255, 140), 1));
        painter.setBrush(QColor(13, 17, 23, 210));
        painter.drawRoundedRect(hint_rect, 6, 6);

        painter.setPen(QColor(88, 166, 255));
        painter.drawText(hint_rect, Qt::AlignCenter, hint);
    }

    // 4. 绘制检测跟踪框与足底点 (高科技 HUD 视觉效果)
    painter.setRenderHint(QPainter::Antialiasing, true);
    for (const auto& trk : current_tracks_) {
        cv::Point2f top_left(trk.bbox.x, trk.bbox.y);
        QPointF c_tl = frame_to_canvas(top_left);
        float w = trk.bbox.width * scale_;
        float h = trk.bbox.height * scale_;

        // 柔和半透明绿光边框
        painter.setPen(QPen(QColor(46, 213, 115, 200), 2));
        painter.setBrush(QColor(46, 213, 115, 20));
        painter.drawRoundedRect(QRectF(c_tl.x(), c_tl.y(), w, h), 4, 4);

        // 四角 HUD 科技高亮小拐角
        float corner_len = std::min(w, h) * 0.25f;
        painter.setPen(QPen(QColor(0, 230, 118), 3));
        // 左上角
        painter.drawLine(QPointF(c_tl.x(), c_tl.y()), QPointF(c_tl.x() + corner_len, c_tl.y()));
        painter.drawLine(QPointF(c_tl.x(), c_tl.y()), QPointF(c_tl.x(), c_tl.y() + corner_len));
        // 右下角
        painter.drawLine(QPointF(c_tl.x() + w, c_tl.y() + h), QPointF(c_tl.x() + w - corner_len, c_tl.y() + h));
        painter.drawLine(QPointF(c_tl.x() + w, c_tl.y() + h), QPointF(c_tl.x() + w, c_tl.y() + h - corner_len));

        // 足底接触点 (外层光晕 + 内部红点)
        QPointF c_foot = frame_to_canvas(trk.foot_point);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 71, 87, 80));
        painter.drawEllipse(c_foot, 7, 7);
        painter.setBrush(QColor(255, 71, 87, 230));
        painter.drawEllipse(c_foot, 3.5, 3.5);

        // 顶部属性气泡胶囊标签
        QString tag = QString("ID:%1 [%2]").arg(trk.track_id).arg(QString::fromStdString(trk.gender));
        QFontMetrics fm(painter.font());
        int text_w = fm.horizontalAdvance(tag) + 12;
        int text_h = 20;
        QRectF tag_rect(c_tl.x(), c_tl.y() - text_h - 4, text_w, text_h);

        painter.setBrush(QColor(22, 27, 34, 210));
        painter.setPen(QPen(QColor(48, 54, 61), 1));
        painter.drawRoundedRect(tag_rect, 3, 3);

        painter.setPen(QColor(240, 246, 252));
        painter.drawText(tag_rect, Qt::AlignCenter, tag);
    }

    // 5. 绘制客流超载赛博红光呼吸律动边框与醒目警报胶囊 (Crowd Surge Warning)
    if (crowd_surge_active_) {
        float norm = 0.5f + 0.5f * std::sin(surge_anim_phase_); // 0.0 ~ 1.0 平滑呼吸正弦
        int border_alpha = static_cast<int>(90 + 150 * norm);   // 90 ~ 240
        int glow_alpha = static_cast<int>(20 + 65 * norm);      // 20 ~ 85

        QRectF video_box(offset_x_, offset_y_, render_w, render_h);

        // 5.1 绘制四边柔和内发光暗红光晕
        QLinearGradient top_glow(QPointF(0, offset_y_), QPointF(0, offset_y_ + 24));
        top_glow.setColorAt(0.0, QColor(248, 81, 73, glow_alpha));
        top_glow.setColorAt(1.0, QColor(248, 81, 73, 0));
        painter.fillRect(QRectF(offset_x_, offset_y_, render_w, 24), top_glow);

        QLinearGradient btm_glow(QPointF(0, offset_y_ + render_h), QPointF(0, offset_y_ + render_h - 24));
        btm_glow.setColorAt(0.0, QColor(248, 81, 73, glow_alpha));
        btm_glow.setColorAt(1.0, QColor(248, 81, 73, 0));
        painter.fillRect(QRectF(offset_x_, offset_y_ + render_h - 24, render_w, 24), btm_glow);

        QLinearGradient left_glow(QPointF(offset_x_, 0), QPointF(offset_x_ + 24, 0));
        left_glow.setColorAt(0.0, QColor(248, 81, 73, glow_alpha));
        left_glow.setColorAt(1.0, QColor(248, 81, 73, 0));
        painter.fillRect(QRectF(offset_x_, offset_y_, 24, render_h), left_glow);

        QLinearGradient right_glow(QPointF(offset_x_ + render_w, 0), QPointF(offset_x_ + render_w - 24, 0));
        right_glow.setColorAt(0.0, QColor(248, 81, 73, glow_alpha));
        right_glow.setColorAt(1.0, QColor(248, 81, 73, 0));
        painter.fillRect(QRectF(offset_x_ + render_w - 24, offset_y_, 24, render_h), right_glow);

        // 5.2 绘制全视口外边框
        painter.setPen(QPen(QColor(248, 81, 73, border_alpha), 3));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(video_box);

        // 5.3 四角高科技警报支架
        float corner_sz = 32.0f;
        painter.setPen(QPen(QColor(255, 71, 87, 255), 4));
        // 左上角
        painter.drawLine(QPointF(offset_x_, offset_y_), QPointF(offset_x_ + corner_sz, offset_y_));
        painter.drawLine(QPointF(offset_x_, offset_y_), QPointF(offset_x_, offset_y_ + corner_sz));
        // 右上角
        painter.drawLine(QPointF(offset_x_ + render_w, offset_y_), QPointF(offset_x_ + render_w - corner_sz, offset_y_));
        painter.drawLine(QPointF(offset_x_ + render_w, offset_y_), QPointF(offset_x_ + render_w, offset_y_ + corner_sz));
        // 左下角
        painter.drawLine(QPointF(offset_x_, offset_y_ + render_h), QPointF(offset_x_ + corner_sz, offset_y_ + render_h));
        painter.drawLine(QPointF(offset_x_, offset_y_ + render_h), QPointF(offset_x_, offset_y_ + render_h - corner_sz));
        // 右下角
        painter.drawLine(QPointF(offset_x_ + render_w, offset_y_ + render_h), QPointF(offset_x_ + render_w - corner_sz, offset_y_ + render_h));
        painter.drawLine(QPointF(offset_x_ + render_w, offset_y_ + render_h), QPointF(offset_x_ + render_w, offset_y_ + render_h - corner_sz));

        // 5.4 顶部悬浮警报胶囊徽章
        QString alert_text = QString("⚠️  区域人员超载告警 (当前: %1 人 / 预警上限: %2 人)")
                                 .arg(surge_occupancy_).arg(surge_threshold_);
        QFont alert_font("Segoe UI", 11, QFont::Bold);
        painter.setFont(alert_font);
        QFontMetrics fm(alert_font);
        int text_w = fm.horizontalAdvance(alert_text) + 32;
        int text_h = 32;
        QRectF badge_rect(offset_x_ + (render_w - text_w) / 2.0f, offset_y_ + 12, text_w, text_h);

        painter.setPen(QPen(QColor(248, 81, 73, border_alpha), 2));
        painter.setBrush(QColor(33, 10, 14, 230));
        painter.drawRoundedRect(badge_rect, 6, 6);

        painter.setPen(QColor(255, 120, 120));
        painter.drawText(badge_rect, Qt::AlignCenter, alert_text);
    }
}

void VideoCanvas::mousePressEvent(QMouseEvent* event) {
    if (read_only_ || event->button() != Qt::LeftButton || !is_point_in_video_rect(event->position())) {
        return;
    }

    QPointF mouse_pos = event->position();
    last_mouse_pos_ = mouse_pos;

    if (mode_ == EditMode::DRAWING) {
        editing_roi_.vertices.push_back(canvas_to_frame(mouse_pos));
        emit polygon_status_changed(validate_polygon(editing_roi_.vertices), editing_roi_.vertices.size());
        update();
        return;
    }

    if (mode_ == EditMode::EDITING) {
        int idx = get_hovered_vertex_index(mouse_pos);
        if (idx != -1) {
            dragging_vertex_idx_ = idx;
        } else {
            std::vector<cv::Point2f> pts = editing_roi_.vertices;
            cv::Point2f cur_f = canvas_to_frame(mouse_pos);
            if (cv::pointPolygonTest(pts, cur_f, false) >= 0) {
                is_dragging_whole_ = true;
            }
        }
    }
}

void VideoCanvas::mouseDoubleClickEvent(QMouseEvent* event) {
    if (read_only_) return;

    if (mode_ == EditMode::DRAWING && editing_roi_.vertices.size() >= 3) {
        mode_ = EditMode::EDITING;
        editing_roi_.is_valid = validate_polygon(editing_roi_.vertices);
        emit polygon_status_changed(editing_roi_.is_valid, editing_roi_.vertices.size());
        update();
    }
}

void VideoCanvas::mouseMoveEvent(QMouseEvent* event) {
    if (read_only_) return;

    QPointF mouse_pos = event->position();
    current_mouse_hover_ = mouse_pos;

    if (mode_ == EditMode::DRAWING) {
        update();
        return;
    }

    if (mode_ == EditMode::EDITING) {
        if (dragging_vertex_idx_ != -1) {
            editing_roi_.vertices[dragging_vertex_idx_] = canvas_to_frame(mouse_pos);
            update();
            return;
        } else if (is_dragging_whole_) {
            cv::Point2f p_now = canvas_to_frame(mouse_pos);
            cv::Point2f p_last = canvas_to_frame(last_mouse_pos_);
            cv::Point2f delta = p_now - p_last;
            for (auto& v : editing_roi_.vertices) {
                v += delta;
                v.x = std::clamp(v.x, 0.0f, video_w_ - 1.0f);
                v.y = std::clamp(v.y, 0.0f, video_h_ - 1.0f);
            }
            last_mouse_pos_ = mouse_pos;
            update();
            return;
        }

        if (get_hovered_vertex_index(mouse_pos) != -1) {
            setCursor(Qt::PointingHandCursor);
        } else {
            setCursor(Qt::ArrowCursor);
        }
    }
}

void VideoCanvas::mouseReleaseEvent(QMouseEvent* event) {
    if (read_only_) return;

    if (event->button() == Qt::LeftButton) {
        dragging_vertex_idx_ = -1;
        is_dragging_whole_ = false;
        if (mode_ == EditMode::EDITING) {
            editing_roi_.is_valid = validate_polygon(editing_roi_.vertices);
            emit polygon_status_changed(editing_roi_.is_valid, editing_roi_.vertices.size());
        }
    }
}

bool VideoCanvas::validate_polygon(const std::vector<cv::Point2f>& pts) const {
    if (pts.size() < 3) return false;
    double area = cv::contourArea(pts);
    return area > 400.0;
}

void VideoCanvas::start_drawing() {
    editing_roi_.vertices.clear();
    editing_roi_.is_valid = false;
    mode_ = EditMode::DRAWING;
    emit polygon_status_changed(false, 0);
    update();
}

void VideoCanvas::reset_roi() {
    editing_roi_.vertices.clear();
    editing_roi_.is_valid = false;
    if (active_roi_idx_ >= 0 && active_roi_idx_ < static_cast<int>(rois_.size())) {
        rois_[active_roi_idx_].vertices.clear();
        rois_[active_roi_idx_].is_valid = false;
    }
    active_roi_ = editing_roi_;
    mode_ = EditMode::IDLE;
    emit roi_applied(active_roi_);
    emit rois_applied(rois_);
    emit polygon_status_changed(false, 0);
    update();
}

void VideoCanvas::apply_changes() {
    if (validate_polygon(editing_roi_.vertices)) {
        editing_roi_.version++;
        editing_roi_.ref_width = static_cast<int>(video_w_ > 0 ? video_w_ : 1920);
        editing_roi_.ref_height = static_cast<int>(video_h_ > 0 ? video_h_ : 1080);
        editing_roi_.is_valid = true;
        if (rois_.empty()) {
            rois_.push_back(editing_roi_);
            active_roi_idx_ = 0;
        } else if (active_roi_idx_ >= 0 && active_roi_idx_ < static_cast<int>(rois_.size())) {
            rois_[active_roi_idx_] = editing_roi_;
        }
        active_roi_ = editing_roi_;
        mode_ = EditMode::IDLE;
        setCursor(Qt::ArrowCursor);
        emit roi_applied(active_roi_);
        emit rois_applied(rois_);
        update();
    }
}

void VideoCanvas::cancel_changes() {
    if (active_roi_idx_ >= 0 && active_roi_idx_ < static_cast<int>(rois_.size())) {
        editing_roi_ = rois_[active_roi_idx_];
    } else {
        editing_roi_ = active_roi_;
    }
    mode_ = EditMode::IDLE;
    setCursor(Qt::ArrowCursor);
    update();
    emit polygon_status_changed(editing_roi_.is_valid, static_cast<int>(editing_roi_.vertices.size()));
}

QPointF VideoCanvas::frame_to_canvas(const cv::Point2f& pt_f) const {
    float x = pt_f.x;
    float y = pt_f.y;
    const auto& roi = (mode_ == EditMode::IDLE) ? active_roi_ : editing_roi_;
    if (roi.ref_width > 0 && video_w_ > 0 && roi.ref_width != static_cast<int>(video_w_)) {
        x = x * video_w_ / static_cast<float>(roi.ref_width);
    }
    if (roi.ref_height > 0 && video_h_ > 0 && roi.ref_height != static_cast<int>(video_h_)) {
        y = y * video_h_ / static_cast<float>(roi.ref_height);
    }
    return QPointF(x * scale_ + offset_x_, y * scale_ + offset_y_);
}

cv::Point2f VideoCanvas::canvas_to_frame(const QPointF& pt_c) const {
    float x = (pt_c.x() - offset_x_) / scale_;
    float y = (pt_c.y() - offset_y_) / scale_;
    x = std::clamp(x, 0.0f, video_w_ - 1.0f);
    y = std::clamp(y, 0.0f, video_h_ - 1.0f);
    return cv::Point2f(x, y);
}

bool VideoCanvas::is_point_in_video_rect(const QPointF& pt_c) const {
    return (pt_c.x() >= offset_x_ && pt_c.x() <= offset_x_ + video_w_ * scale_ &&
            pt_c.y() >= offset_y_ && pt_c.y() <= offset_y_ + video_h_ * scale_);
}

int VideoCanvas::get_hovered_vertex_index(const QPointF& pt_c) const {
    const auto& roi = (mode_ == EditMode::IDLE) ? active_roi_ : editing_roi_;
    const float hit_radius_sq = 64.0f;
    for (int i = 0; i < static_cast<int>(roi.vertices.size()); ++i) {
        QPointF v_c = frame_to_canvas(roi.vertices[i]);
        float dx = pt_c.x() - v_c.x();
        float dy = pt_c.y() - v_c.y();
        if (dx * dx + dy * dy <= hit_radius_sq) {
            return i;
        }
    }
    return -1;
}

void VideoCanvas::draw_standby_screen(QPainter& painter) {
    painter.fillRect(rect(), QColor(13, 17, 23)); // #0d1117 深黑曜石底色

    int w = std::min(width() - 40, 680);
    int h = std::min(height() - 40, 360);
    int cx = (width() - w) / 2;
    int cy = (height() - h) / 2;

    QRect card_rect(cx, cy, w, h);

    // 1. 卡片背景与圆角细边框
    painter.setBrush(QColor(22, 27, 34)); // #161b22
    painter.setPen(QPen(QColor(48, 54, 61), 1)); // #30363d
    painter.drawRoundedRect(card_rect, 10, 10);

    // 顶部青色光条
    painter.setBrush(QColor(88, 166, 255)); // #58a6ff
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(QRect(cx, cy, w, 3), 2, 2);

    int content_x = cx + 28;
    int content_y = cy + 28;

    // 2. 待机状态芯片标签
    QRect pill_rect(content_x, content_y, 110, 24);
    painter.setBrush(QColor(33, 38, 45));
    painter.setPen(QPen(QColor(56, 139, 253, 100), 1));
    painter.drawRoundedRect(pill_rect, 12, 12);

    painter.setBrush(QColor(240, 136, 62)); // 琥珀黄圆点
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(content_x + 10, content_y + 8, 8, 8);

    QFont pill_font("Segoe UI", 9, QFont::DemiBold);
    painter.setFont(pill_font);
    painter.setPen(QColor(240, 136, 62));
    painter.drawText(QRect(content_x + 24, content_y, 80, 24), Qt::AlignVCenter, "待机就绪");

    // 3. 主标题 (全矢量 TrueType 字体)
    content_y += 36;
    QFont title_font("Microsoft YaHei", 16, QFont::Bold);
    title_font.setStyleHint(QFont::SansSerif);
    painter.setFont(title_font);
    painter.setPen(QColor(240, 246, 252));
    painter.drawText(content_x, content_y + 20, "FlowSight AI 智能客流感知系统");

    // 4. 副标题
    content_y += 30;
    QFont sub_font("Microsoft YaHei", 10);
    painter.setFont(sub_font);
    painter.setPen(QColor(139, 148, 158));
    painter.drawText(content_x, content_y + 16, "当前无活动视频输入源，请选择接入本地 MP4 视频或网络摄像机流");

    // 5. 操作引导框
    content_y += 32;
    QRect guide_rect(content_x, content_y, w - 56, 76);
    painter.setBrush(QColor(13, 17, 23));
    painter.setPen(QPen(QColor(48, 54, 61), 1));
    painter.drawRoundedRect(guide_rect, 6, 6);

    QFont guide_font("Microsoft YaHei", 9, QFont::Normal);
    painter.setFont(guide_font);
    painter.setPen(QColor(201, 209, 217));
    painter.drawText(content_x + 14, content_y + 26, "• 播放本地视频: 点击上方【 📁 载入 MP4 视频... 】按钮，选择任意 MP4 文件实时分析");
    painter.drawText(content_x + 14, content_y + 54, "• 接入网络摄像头: 切换至左侧【 ⚙️ 系统设置 】页面，配置标准 RTSP / RTMP 流地址");

    // 6. 核心引擎状态徽章
    content_y += 94;
    auto draw_badge = [&](int bx, int by, const QString& text, const QColor& dot_color) {
        painter.setBrush(dot_color);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(bx, by + 4, 7, 7);

        QFont b_font("Segoe UI", 9, QFont::Medium);
        painter.setFont(b_font);
        painter.setPen(QColor(139, 148, 158));
        painter.drawText(bx + 14, by + 12, text);
    };

    draw_badge(content_x, content_y, "YOLOv8n ONNX 深度学习模型: 已就绪", QColor(63, 185, 80));
    draw_badge(content_x + 280, content_y, "ByteTrack 跟踪与防抖状态机: 已就绪", QColor(63, 185, 80));
}
