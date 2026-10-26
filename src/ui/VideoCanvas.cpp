#include "VideoCanvas.hpp"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QDateTime>
#include <cmath>
#include <opencv2/imgproc.hpp>
#include <algorithm>

VideoCanvas::VideoCanvas(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(480, 270);

    surge_anim_timer_ = new QTimer(this);
    surge_anim_timer_->setInterval(30);
    connect(surge_anim_timer_, &QTimer::timeout, this, [this]() {
        bool need_update = false;
        if (crowd_surge_active_) {
            surge_anim_phase_ += 0.12f;
            if (surge_anim_phase_ > 6.2831853f) surge_anim_phase_ -= 6.2831853f;
            need_update = true;
        }
        if (!active_passage_anims_.empty()) {
            need_update = true;
        }
        if (need_update) {
            update();
        } else if (!crowd_surge_active_) {
            surge_anim_timer_->stop();
        }
    });
}

VideoCanvas::~VideoCanvas() {
    if (surge_anim_timer_) {
        surge_anim_timer_->stop();
    }
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

void VideoCanvas::set_active_roi_flow_angle(float deg) {
    if (active_roi_idx_ >= 0 && active_roi_idx_ < static_cast<int>(rois_.size())) {
        rois_[active_roi_idx_].flow_angle_deg = deg;
        editing_roi_.flow_angle_deg = deg;
        active_roi_.flow_angle_deg = deg;
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

            // 计算进方向法向矢量 (支持 0°~360° 任意航向角与反转朝向)
            float rad = roi.flow_angle_deg * 3.14159265358979323846f / 180.0f;
            QPointF base_dir(std::cos(rad), std::sin(rad));
            if (roi.invert_direction) base_dir = -base_dir;

            // 双向形象化罗盘指示系统:
            // 🟢 顺向 = 进入 [IN] (绿色粗矢量箭头 + 胶囊标牌)
            // 🔴 逆向 = 离开 [OUT] (红色粗矢量箭头 + 胶囊标牌)
            // 📍 中心 = 防区名称卡片
            QPointF perp(-base_dir.y(), base_dir.x());
            float perp_offset = 38.0f;
            QPointF pos_in = centroid - perp * perp_offset;
            QPointF pos_out = centroid + perp * perp_offset;

            auto draw_dir_indicator = [&](const QPointF& origin, const QPointF& dir, const QString& label, const QColor& color, const QColor& bg_color) {
                float len = 42.0f;
                QPointF p_start = origin - dir * (len * 0.45f);
                QPointF p_end = origin + dir * (len * 0.55f);

                // 发光外圈底线
                QColor halo_col = color;
                halo_col.setAlpha(80);
                painter.setPen(QPen(halo_col, 5.5f, Qt::SolidLine, Qt::RoundCap));
                painter.drawLine(p_start, p_end);

                // 实体粗主杆
                painter.setPen(QPen(color, 3.2f, Qt::SolidLine, Qt::RoundCap));
                painter.drawLine(p_start, p_end);

                // 起点圆环锚点
                painter.setPen(Qt::NoPen);
                painter.setBrush(halo_col);
                painter.drawEllipse(p_start, 5.0, 5.0);
                painter.setBrush(color);
                painter.drawEllipse(p_start, 2.8, 2.8);

                // 箭头几何翼 (立体Chevron角)
                QPointF p_perp(-dir.y(), dir.x());
                float w_len = 12.0f; float w_w = 7.0f;
                QPointF w1 = p_end - dir * w_len + p_perp * w_w;
                QPointF w2 = p_end - dir * w_len - p_perp * w_w;
                QPolygonF arr_head;
                arr_head << p_end << w1 << (p_end - dir * (w_len * 0.65f)) << w2;
                painter.setBrush(color);
                painter.drawPolygon(arr_head);

                // 明确文字胶囊徽标 [🟢 进入 IN] 或 [🔴 离开 OUT]
                QFont font("Segoe UI", 9, QFont::Bold);
                painter.setFont(font);
                QFontMetrics fm(font);
                int tag_w = fm.horizontalAdvance(label) + 14;
                int tag_h = 20;

                QPointF tag_center = p_start - dir * 14.0f;
                QRectF tag_rect(tag_center.x() - tag_w / 2.0f, tag_center.y() - tag_h / 2.0f, tag_w, tag_h);

                painter.setPen(QPen(color, 1.2f));
                painter.setBrush(bg_color);
                painter.drawRoundedRect(tag_rect, 4, 4);

                painter.setPen(color);
                painter.drawText(tag_rect, Qt::AlignCenter, label);
            };

            // 绘制绿色进入指示与红色离开指示
            draw_dir_indicator(pos_in, base_dir, "🟢 进入 [IN]", QColor("#2ed573"), QColor(8, 28, 18, 225));
            draw_dir_indicator(pos_out, -base_dir, "🔴 离开 [OUT]", QColor("#ff4757"), QColor(32, 10, 14, 225));

            // 防区中心绘制醒目名牌卡片
            QString name_str = QString::fromStdString(roi.name.empty() ? QString("防区 %1").arg(roi.roi_id).toStdString() : roi.name);
            QString center_title = QString("📍 %1").arg(name_str);
            QFont c_font("Microsoft YaHei", 9, QFont::Bold);
            painter.setFont(c_font);
            QFontMetrics cfm(c_font);
            int c_w = cfm.horizontalAdvance(center_title) + 16;
            int c_h = 22;
            QRectF c_rect(centroid.x() - c_w / 2.0f, centroid.y() - c_h / 2.0f, c_w, c_h);

            painter.setPen(QPen(base_color, 1.5f));
            painter.setBrush(QColor(13, 17, 23, 230));
            painter.drawRoundedRect(c_rect, 4, 4);

            painter.setPen(QColor(240, 246, 252));
            painter.drawText(c_rect, Qt::AlignCenter, center_title);

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

        QColor border_color = QColor(46, 213, 115, 200);
        QColor corner_color = QColor(0, 230, 118);
        if (!trk.plate_number.empty()) {
            if (trk.plate_color == "绿牌") {
                border_color = QColor(46, 204, 113, 220);
                corner_color = QColor(39, 174, 96);
            } else if (trk.plate_color == "黄牌") {
                border_color = QColor(241, 196, 15, 220);
                corner_color = QColor(243, 156, 18);
            } else {
                border_color = QColor(52, 152, 219, 220);
                corner_color = QColor(41, 128, 185);
            }
        }

        // 柔和半透明边框
        painter.setPen(QPen(border_color, 2));
        painter.setBrush(QColor(border_color.red(), border_color.green(), border_color.blue(), 25));
        painter.drawRoundedRect(QRectF(c_tl.x(), c_tl.y(), w, h), 4, 4);

        // 四角 HUD 科技高亮小拐角
        float corner_len = std::min(w, h) * 0.25f;
        painter.setPen(QPen(corner_color, 3));
        // 左上角
        painter.drawLine(QPointF(c_tl.x(), c_tl.y()), QPointF(c_tl.x() + corner_len, c_tl.y()));
        painter.drawLine(QPointF(c_tl.x(), c_tl.y()), QPointF(c_tl.x(), c_tl.y() + corner_len));
        // 右下角
        painter.drawLine(QPointF(c_tl.x() + w, c_tl.y() + h), QPointF(c_tl.x() + w - corner_len, c_tl.y() + h));
        painter.drawLine(QPointF(c_tl.x() + w, c_tl.y() + h), QPointF(c_tl.x() + w, c_tl.y() + h - corner_len));

        // 接触代表点 (外层光晕 + 内部红点)
        QPointF c_foot = frame_to_canvas(trk.foot_point);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 71, 87, 80));
        painter.drawEllipse(c_foot, 7, 7);
        painter.setBrush(QColor(255, 71, 87, 230));
        painter.drawEllipse(c_foot, 3.5, 3.5);

        // 顶部属性气泡胶囊标签 (车牌/车型 或 行人性别年龄)
        QString tag;
        if (!trk.plate_number.empty()) {
            QString veh_desc = QString::fromStdString(trk.vehicle_color) + QString::fromStdString(trk.vehicle_type);
            if (veh_desc.isEmpty()) veh_desc = QString::fromStdString(trk.plate_color);
            tag = QString("ID:%1 [%2 %3]").arg(trk.track_id)
                                           .arg(QString::fromStdString(trk.plate_number))
                                           .arg(veh_desc);
        } else if (!trk.age_group.empty() && trk.age_group != "未知") {
            tag = QString("ID:%1 [%2 | %3]").arg(trk.track_id)
                                             .arg(QString::fromStdString(trk.gender))
                                             .arg(QString::fromStdString(trk.age_group));
        } else if (!trk.gender.empty() && trk.gender != "未知") {
            tag = QString("ID:%1 [%2]").arg(trk.track_id).arg(QString::fromStdString(trk.gender));
        } else {
            tag = QString("ID:%1").arg(trk.track_id);
        }
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

    // 6. 绘制实时通行水波纹与升腾气泡动态反馈
    if (!active_passage_anims_.empty()) {
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        std::vector<PassageAnim> remaining;

        for (const auto& anim : active_passage_anims_) {
            qint64 elapsed = now - anim.start_time_ms;
            if (elapsed < 0 || elapsed > 1500) {
                continue; // 超过 1.5 秒淡出移除
            }
            remaining.push_back(anim);

            float t = static_cast<float>(elapsed) / 1500.0f; // 0.0 ~ 1.0
            float alpha_f = (1.0f - t);
            int alpha_255 = static_cast<int>(255 * alpha_f);
            if (alpha_255 <= 0) continue;

            QPointF canvas_pt = frame_to_canvas(anim.foot_point);
            bool is_entry = (anim.direction == PassageDirection::ENTRY);
            QColor base_col = is_entry ? QColor("#2ed573") : QColor("#ff4757");

            painter.save();
            painter.setRenderHint(QPainter::Antialiasing, true);

            // 脚底扩散双层水波纹 (Expanding Ripple Rings)
            float r1 = 8.0f + 42.0f * t;
            float r2 = 4.0f + 24.0f * t;

            QColor ring_col1 = base_col;
            ring_col1.setAlpha(static_cast<int>(180 * alpha_f));
            painter.setPen(QPen(ring_col1, 2.5f * alpha_f + 0.8f));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(canvas_pt, r1, r1);

            QColor ring_col2 = base_col;
            ring_col2.setAlpha(static_cast<int>(220 * alpha_f));
            painter.setPen(QPen(ring_col2, 1.8f * alpha_f + 0.5f));
            painter.drawEllipse(canvas_pt, r2, r2);

            // 升腾文字气泡胶囊 (Rising Floating Tag)
            float rise_offset = 32.0f * t;
            QPointF bubble_pos = canvas_pt - QPointF(0, 24.0f + rise_offset);

            QString anim_txt = is_entry ? QString("🟢 判定进入 [IN] ＋1 ↑")
                                        : QString("🔴 判定离开 [OUT] ＋1 ↓");
            QFont b_font("Segoe UI", 10, QFont::Bold);
            painter.setFont(b_font);
            QFontMetrics bfm(b_font);
            int bw = bfm.horizontalAdvance(anim_txt) + 16;
            int bh = 24;
            QRectF brect(bubble_pos.x() - bw / 2.0f, bubble_pos.y() - bh / 2.0f, bw, bh);

            QColor bg_col(13, 17, 23, static_cast<int>(220 * alpha_f));
            QColor border_col = base_col;
            border_col.setAlpha(alpha_255);
            painter.setPen(QPen(border_col, 1.5f));
            painter.setBrush(bg_col);
            painter.drawRoundedRect(brect, 4, 4);

            painter.setPen(border_col);
            painter.drawText(brect, Qt::AlignCenter, anim_txt);

            painter.restore();
        }

        active_passage_anims_ = std::move(remaining);
    }

    // 7. 绘制标准安防通行方向图例
    draw_flow_legend(painter, offset_x_, offset_y_, render_w, render_h);
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
    if (read_only_) {
        emit double_clicked();
        event->accept();
        return;
    }

    if (mode_ == EditMode::DRAWING && editing_roi_.vertices.size() >= 3) {
        mode_ = EditMode::EDITING;
        editing_roi_.is_valid = validate_polygon(editing_roi_.vertices);
        emit polygon_status_changed(editing_roi_.is_valid, editing_roi_.vertices.size());
        update();
    }
}

void VideoCanvas::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        emit escape_pressed();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
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
    int w = video_w_ > 0 ? static_cast<int>(video_w_) : 1920;
    int h = video_h_ > 0 ? static_cast<int>(video_h_) : 1080;
    RoiConfig def_roi = make_default_corridor_roi(w, h, 1, "A门主通道", "#58a6ff");

    if (active_roi_idx_ >= 0 && active_roi_idx_ < static_cast<int>(rois_.size())) {
        rois_[active_roi_idx_] = def_roi;
    } else {
        rois_ = {def_roi};
        active_roi_idx_ = 0;
    }
    active_roi_ = def_roi;
    editing_roi_ = def_roi;
    mode_ = EditMode::IDLE;
    emit roi_applied(active_roi_);
    emit rois_applied(rois_);
    emit polygon_status_changed(true, static_cast<int>(active_roi_.vertices.size()));
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
    return QPointF(pt_f.x * scale_ + offset_x_, pt_f.y * scale_ + offset_y_);
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
        float vx = roi.vertices[i].x;
        float vy = roi.vertices[i].y;
        if (roi.ref_width > 0 && video_w_ > 0 && roi.ref_width != static_cast<int>(video_w_)) {
            vx = vx * video_w_ / static_cast<float>(roi.ref_width);
        }
        if (roi.ref_height > 0 && video_h_ > 0 && roi.ref_height != static_cast<int>(video_h_)) {
            vy = vy * video_h_ / static_cast<float>(roi.ref_height);
        }
        QPointF v_c(vx * scale_ + offset_x_, vy * scale_ + offset_y_);
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

void VideoCanvas::trigger_passage_animation(int track_id, PassageDirection dir, float foot_x, float foot_y) {
    PassageAnim anim;
    anim.track_id = track_id;
    anim.direction = dir;
    anim.foot_point = cv::Point2f(foot_x, foot_y);
    anim.start_time_ms = QDateTime::currentMSecsSinceEpoch();
    active_passage_anims_.push_back(anim);

    if (surge_anim_timer_ && !surge_anim_timer_->isActive()) {
        surge_anim_timer_->start();
    }
    update();
}

void VideoCanvas::draw_flow_legend(QPainter& painter, float render_x, float render_y, float render_w, float render_h) {
    if (rois_.empty() && !editing_roi_.is_valid) return;

    // 常驻左下角安防工业级图例卡片
    int legend_w = 264;
    int legend_h = 30;
    float lx = render_x + 14.0f;
    float ly = render_y + render_h - legend_h - 14.0f;
    QRectF lrect(lx, ly, legend_w, legend_h);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    // 半透明深色底卡与微边框
    painter.setPen(QPen(QColor(48, 54, 61, 200), 1));
    painter.setBrush(QColor(13, 17, 23, 225));
    painter.drawRoundedRect(lrect, 4, 4);

    // 图标与文字
    QFont font("Microsoft YaHei", 9, QFont::DemiBold);
    painter.setFont(font);

    // 🧭 通行方向:
    painter.setPen(QColor(139, 148, 158));
    painter.drawText(QRectF(lx + 8, ly, 74, legend_h), Qt::AlignVCenter, "🧭 通行方向:");

    // 🟢 进入 IN
    painter.setPen(QColor("#2ed573"));
    painter.drawText(QRectF(lx + 86, ly, 76, legend_h), Qt::AlignVCenter, "🟢 进入 [IN]");

    // 分隔线
    painter.setPen(QColor(70, 80, 95));
    painter.drawText(QRectF(lx + 164, ly, 12, legend_h), Qt::AlignCenter, "│");

    // 🔴 离开 OUT
    painter.setPen(QColor("#ff4757"));
    painter.drawText(QRectF(lx + 178, ly, 78, legend_h), Qt::AlignVCenter, "🔴 离开 [OUT]");

    painter.restore();
}

