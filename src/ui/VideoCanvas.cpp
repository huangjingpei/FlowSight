#include "VideoCanvas.hpp"
#include <QPainter>
#include <QMouseEvent>
#include <opencv2/imgproc.hpp>
#include <algorithm>

VideoCanvas::VideoCanvas(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setMinimumSize(480, 270);
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

void VideoCanvas::set_roi(const RoiConfig& roi) {
    active_roi_ = roi;
    editing_roi_ = roi;
    update();
}

RoiConfig VideoCanvas::get_roi() const {
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

    // 3. 绘制 ROI 多边形
    const auto& roi = (mode_ == EditMode::IDLE) ? active_roi_ : editing_roi_;
    if (!roi.vertices.empty()) {
        QPolygonF poly;
        for (const auto& v : roi.vertices) {
            poly << frame_to_canvas(v);
        }

        if (mode_ == EditMode::DRAWING) {
            painter.setPen(QPen(Qt::yellow, 2, Qt::DashLine));
            painter.drawPolyline(poly);
            if (!poly.isEmpty()) {
                painter.drawLine(poly.last(), current_mouse_hover_);
            }
            painter.setBrush(Qt::cyan);
            for (const auto& pt : poly) {
                painter.drawEllipse(pt, 5, 5);
            }
        } else if (poly.size() >= 3) {
            // 半透明黄色填充
            painter.setBrush(QColor(255, 215, 0, 50));
            painter.setPen(QPen(QColor(255, 215, 0, 220), 2, Qt::SolidLine));
            painter.drawPolygon(poly);

            // 标定编辑模式下呈现可拖拽控制点
            if (mode_ == EditMode::EDITING && !read_only_) {
                painter.setBrush(Qt::white);
                painter.setPen(QPen(Qt::red, 2));
                for (int i = 0; i < poly.size(); ++i) {
                    painter.drawEllipse(poly[i], 6, 6);
                }
            }
        }
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
    active_roi_.vertices.clear();
    active_roi_.is_valid = false;
    mode_ = EditMode::IDLE;
    emit roi_applied(active_roi_);
    update();
}

void VideoCanvas::apply_changes() {
    if (validate_polygon(editing_roi_.vertices)) {
        active_roi_ = editing_roi_;
        active_roi_.version++;
        active_roi_.ref_width = static_cast<int>(video_w_);
        active_roi_.ref_height = static_cast<int>(video_h_);
        active_roi_.is_valid = true;
        mode_ = EditMode::IDLE;
        emit roi_applied(active_roi_);
        update();
    }
}

void VideoCanvas::cancel_changes() {
    editing_roi_ = active_roi_;
    mode_ = EditMode::IDLE;
    update();
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
