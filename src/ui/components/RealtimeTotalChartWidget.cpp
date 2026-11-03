#include "RealtimeTotalChartWidget.hpp"
#include "storage/ConfigManager.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QDateTime>
#include <algorithm>
#include <cmath>

RealtimeTotalChartWidget::RealtimeTotalChartWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(320);
    setMouseTracking(true);

    // 默认预填充过去 60 秒的基础平稳时序点，避免刚启动时界面空白
    qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
    for (int i = window_span_sec_; i >= 0; --i) {
        history_.push_back({now_ms - i * 1000, 0, 0, 0});
    }

    sample_timer_ = new QTimer(this);
    sample_timer_->setInterval(1000);
    connect(sample_timer_, &QTimer::timeout, this, &RealtimeTotalChartWidget::on_timer_tick);
    sample_timer_->start();
}

void RealtimeTotalChartWidget::set_counts(int total, int entry, int exit) {
    current_total_ = total;
    current_entry_ = entry;
    current_exit_ = exit;

    // 若当前历史记录均为空，同步初始化基准值
    if (!history_.empty() && history_.back().total == 0 && total > 0) {
        for (auto& pt : history_) {
            pt.total = total;
            pt.entry = entry;
            pt.exit = exit;
        }
    }
    update();
}

void RealtimeTotalChartWidget::add_passage(const PassageEvent& ev) {
    if (ev.direction == PassageDirection::ENTRY) {
        current_entry_++;
    } else if (ev.direction == PassageDirection::EXIT) {
        current_exit_++;
    }
    current_total_ = current_entry_ + current_exit_;
    update();
}

void RealtimeTotalChartWidget::reset_data() {
    current_total_ = 0;
    current_entry_ = 0;
    current_exit_ = 0;
    history_.clear();
    qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
    for (int i = window_span_sec_; i >= 0; --i) {
        history_.push_back({now_ms - i * 1000, 0, 0, 0});
    }
    hovered_idx_ = -1;
    update();
}

void RealtimeTotalChartWidget::on_timer_tick() {
    qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
    history_.push_back({now_ms, current_total_, current_entry_, current_exit_});

    // 保留最近 window_span_sec_ 秒
    qint64 cutoff_ms = now_ms - window_span_sec_ * 1000;
    while (!history_.empty() && history_.front().time_ms < cutoff_ms) {
        history_.pop_front();
    }
    update();
}

void RealtimeTotalChartWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // 1. 底板容器背景与科技圆角边框
    QRect full_rect = rect().adjusted(1, 1, -1, -1);
    p.setPen(QPen(QColor("#30363d"), 1));
    p.setBrush(QColor("#161b22"));
    p.drawRoundedRect(full_rect, 8, 8);

    // 2. 顶部标题栏
    bool is_veh = ConfigManager::instance().is_vehicle_mode();
    QString title = is_veh ? "📈 实时总计车流动态走势" : "📈 实时总计人数动态走势";
    QString unit = is_veh ? "车次" : "人次";
    p.setFont(QFont("Segoe UI", 11, QFont::Bold));
    p.setPen(QColor("#58a6ff"));
    p.drawText(20, 26, title);

    int title_w = p.fontMetrics().horizontalAdvance(title);

    // 顶部实时小芯片
    int chip_x = 20 + title_w + 30;
    p.setFont(QFont("Segoe UI", 9));
    
    // 当前总计
    p.setPen(QColor("#388bfd"));
    p.drawText(chip_x, 26, QString("总计: %1 %2").arg(current_total_).arg(unit));
    chip_x += p.fontMetrics().horizontalAdvance(QString("总计: %1 %2").arg(current_total_).arg(unit)) + 16;

    // 进
    p.setPen(QColor("#2ea043"));
    p.drawText(chip_x, 26, QString("%1: %2").arg(is_veh ? "进车" : "进").arg(current_entry_));
    chip_x += p.fontMetrics().horizontalAdvance(QString("%1: %2").arg(is_veh ? "进车" : "进").arg(current_entry_)) + 16;

    // 出
    p.setPen(QColor("#f85149"));
    p.drawText(chip_x, 26, QString("%1: %2").arg(is_veh ? "出车" : "出").arg(current_exit_));
    chip_x += p.fontMetrics().horizontalAdvance(QString("%1: %2").arg(is_veh ? "出车" : "出").arg(current_exit_)) + 16;

    // 净增
    int net = current_entry_ - current_exit_;
    p.setPen(net >= 0 ? QColor("#3fb950") : QColor("#ffa502"));
    p.drawText(chip_x, 26, QString("净增: %1%2").arg(net > 0 ? "+" : "").arg(net));

    // 图例 (右侧对齐)
    int legend_x = width() - 260;
    // 🔵 总计
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#388bfd"));
    p.drawRoundedRect(legend_x, 16, 12, 12, 2, 2);
    p.setPen(QColor("#c9d1d9"));
    p.drawText(legend_x + 16, 27, "总计");

    // 🟢 进
    legend_x += 70;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#2ea043"));
    p.drawRoundedRect(legend_x, 16, 12, 12, 2, 2);
    p.setPen(QColor("#c9d1d9"));
    p.drawText(legend_x + 16, 27, "累计进");

    // 🔴 出
    legend_x += 75;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#f85149"));
    p.drawRoundedRect(legend_x, 16, 12, 12, 2, 2);
    p.setPen(QColor("#c9d1d9"));
    p.drawText(legend_x + 16, 27, "累计出");

    // 3. 计算坐标范围
    int min_val = 0;
    int max_val = 10;
    for (const auto& pt : history_) {
        max_val = std::max(max_val, pt.total);
    }
    // 向上取整留有余量
    max_val = ((max_val + 9) / 10) * 10;

    QRect plot_rect(55, 45, width() - 80, height() - 78);
    if (plot_rect.width() <= 10 || plot_rect.height() <= 10) return;

    // 4. 绘制网格与刻度
    draw_background_and_grid(p, plot_rect, min_val, max_val);

    // 5. 绘制平滑曲线与阴影填充
    draw_curves(p, plot_rect, min_val, max_val);

    // 6. 悬停卡片
    if (hovered_idx_ >= 0 && hovered_idx_ < static_cast<int>(history_.size())) {
        draw_hover_card(p, plot_rect, min_val, max_val);
    }
}

void RealtimeTotalChartWidget::draw_background_and_grid(QPainter& p, const QRect& plot_rect, int min_val, int max_val) {
    p.save();
    QPen grid_pen(QColor("#21262d"), 1, Qt::DashLine);
    p.setFont(QFont("Consolas", 8));

    // Y 轴水平网格 (4等分)
    int grid_lines = 4;
    for (int i = 0; i <= grid_lines; ++i) {
        int y = plot_rect.bottom() - (plot_rect.height() * i) / grid_lines;
        int val = min_val + ((max_val - min_val) * i) / grid_lines;

        p.setPen(grid_pen);
        p.drawLine(plot_rect.left(), y, plot_rect.right(), y);

        p.setPen(QColor("#6e7681"));
        p.drawText(QRect(0, y - 8, plot_rect.left() - 8, 16), Qt::AlignRight | Qt::AlignVCenter, QString::number(val));
    }

    // X 轴时间刻度 (每 15 秒一个标记)
    if (!history_.empty()) {
        qint64 t_start = history_.front().time_ms;
        qint64 t_end = history_.back().time_ms;
        if (t_end <= t_start) t_end = t_start + 1;

        int num_ticks = 4;
        for (int i = 0; i <= num_ticks; ++i) {
            double frac = static_cast<double>(i) / num_ticks;
            int x = plot_rect.left() + static_cast<int>(frac * plot_rect.width());
            qint64 t = t_start + static_cast<qint64>(frac * (t_end - t_start));
            QString time_str = QDateTime::fromMSecsSinceEpoch(t).toString("hh:mm:ss");

            p.setPen(grid_pen);
            p.drawLine(x, plot_rect.top(), x, plot_rect.bottom());

            p.setPen(QColor("#8b949e"));
            p.drawText(QRect(x - 30, plot_rect.bottom() + 6, 60, 16), Qt::AlignCenter, time_str);
        }
    }
    p.restore();
}

namespace {
    // 基于 Fritsch-Carlson 算法的保单调三次贝塞尔平滑样条曲线生成器
    void build_monotone_cubic_path(const std::vector<QPointF>& points,
                                   double baseline_y,
                                   QPainterPath& path_curve,
                                   QPainterPath* path_fill,
                                   double min_y, double max_y) {
        if (points.empty()) return;

        path_curve.moveTo(points.front());
        if (path_fill) {
            path_fill->moveTo(points.front().x(), baseline_y);
            path_fill->lineTo(points.front());
        }

        if (points.size() == 1) {
            if (path_fill) {
                path_fill->lineTo(points.front().x(), baseline_y);
                path_fill->closeSubpath();
            }
            return;
        }

        if (points.size() == 2) {
            path_curve.lineTo(points.back());
            if (path_fill) {
                path_fill->lineTo(points.back());
                path_fill->lineTo(points.back().x(), baseline_y);
                path_fill->closeSubpath();
            }
            return;
        }

        const size_t n = points.size();
        std::vector<double> dx(n - 1), dy(n - 1), m(n - 1);
        for (size_t i = 0; i < n - 1; ++i) {
            dx[i] = points[i + 1].x() - points[i].x();
            dy[i] = points[i + 1].y() - points[i].y();
            m[i] = (std::abs(dx[i]) > 1e-5) ? (dy[i] / dx[i]) : 0.0;
        }

        std::vector<double> tangents(n, 0.0);
        tangents[0] = m[0];
        tangents[n - 1] = m[n - 2];
        for (size_t i = 1; i < n - 1; ++i) {
            if (m[i - 1] * m[i] <= 0.0) {
                tangents[i] = 0.0;
            } else {
                tangents[i] = (m[i - 1] + m[i]) / 2.0;
            }
        }

        for (size_t i = 0; i < n - 1; ++i) {
            if (std::abs(m[i]) < 1e-5) {
                tangents[i] = 0.0;
                tangents[i + 1] = 0.0;
            } else {
                double alpha = tangents[i] / m[i];
                double beta = tangents[i + 1] / m[i];
                double dist = alpha * alpha + beta * beta;
                if (dist > 9.0) {
                    double tau = 3.0 / std::sqrt(dist);
                    tangents[i] = tau * alpha * m[i];
                    tangents[i + 1] = tau * beta * m[i];
                }
            }
        }

        for (size_t i = 0; i < n - 1; ++i) {
            double h = dx[i];
            QPointF p1 = points[i];
            QPointF p2 = points[i + 1];
            QPointF c1(p1.x() + h / 3.0, p1.y() + tangents[i] * h / 3.0);
            QPointF c2(p2.x() - h / 3.0, p2.y() - tangents[i + 1] * h / 3.0);

            c1.setY(std::clamp(c1.y(), min_y, max_y));
            c2.setY(std::clamp(c2.y(), min_y, max_y));

            path_curve.cubicTo(c1, c2, p2);
            if (path_fill) {
                path_fill->cubicTo(c1, c2, p2);
            }
        }

        if (path_fill) {
            path_fill->lineTo(points.back().x(), baseline_y);
            path_fill->closeSubpath();
        }
    }
}

void RealtimeTotalChartWidget::draw_curves(QPainter& p, const QRect& plot_rect, int min_val, int max_val) {
    if (history_.size() < 2) return;

    p.save();
    qint64 t_start = history_.front().time_ms;
    qint64 t_end = history_.back().time_ms;
    if (t_end <= t_start) t_end = t_start + 1;

    auto val_to_y = [&](int val) -> double {
        double ratio = static_cast<double>(val - min_val) / std::max(1, (max_val - min_val));
        return plot_rect.bottom() - ratio * plot_rect.height();
    };

    auto time_to_x = [&](qint64 t) -> double {
        double ratio = static_cast<double>(t - t_start) / (t_end - t_start);
        return plot_rect.left() + ratio * plot_rect.width();
    };

    std::vector<QPointF> pts_total, pts_entry, pts_exit;
    pts_total.reserve(history_.size());
    pts_entry.reserve(history_.size());
    pts_exit.reserve(history_.size());

    for (const auto& pt : history_) {
        double x = time_to_x(pt.time_ms);
        pts_total.emplace_back(x, val_to_y(pt.total));
        pts_entry.emplace_back(x, val_to_y(pt.entry));
        pts_exit.emplace_back(x, val_to_y(pt.exit));
    }

    QPainterPath path_total, path_fill, path_entry, path_exit;
    build_monotone_cubic_path(pts_total, plot_rect.bottom(), path_total, &path_fill, plot_rect.top(), plot_rect.bottom());
    build_monotone_cubic_path(pts_entry, plot_rect.bottom(), path_entry, nullptr, plot_rect.top(), plot_rect.bottom());
    build_monotone_cubic_path(pts_exit, plot_rect.bottom(), path_exit, nullptr, plot_rect.top(), plot_rect.bottom());

    // 绘制总计下方的渐变辉光
    QLinearGradient grad(QPointF(0, plot_rect.top()), QPointF(0, plot_rect.bottom()));
    grad.setColorAt(0.0, QColor(56, 139, 253, 60));
    grad.setColorAt(0.5, QColor(56, 139, 253, 20));
    grad.setColorAt(1.0, QColor(56, 139, 253, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(grad);
    p.drawPath(path_fill);

    // 绘制平滑曲线 (抗锯齿圆角端点)
    // 1. 累计进 (绿色)
    QPen pen_entry(QColor("#2ea043"), 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen_entry);
    p.setBrush(Qt::NoBrush);
    p.drawPath(path_entry);

    // 2. 累计出 (红色)
    QPen pen_exit(QColor("#f85149"), 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen_exit);
    p.drawPath(path_exit);

    // 3. 总计 (科技蓝，粗线)
    QPen pen_total(QColor("#388bfd"), 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen_total);
    p.drawPath(path_total);

    // 在最新点绘制发光圆环点
    const auto& latest = history_.back();
    double hx = time_to_x(latest.time_ms);
    double hy = val_to_y(latest.total);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(56, 139, 253, 60));
    p.drawEllipse(QPointF(hx, hy), 7.5, 7.5);
    p.setBrush(QColor("#388bfd"));
    p.drawEllipse(QPointF(hx, hy), 4.0, 4.0);
    p.setBrush(QColor("#ffffff"));
    p.drawEllipse(QPointF(hx, hy), 2.0, 2.0);

    p.restore();
}

void RealtimeTotalChartWidget::draw_hover_card(QPainter& p, const QRect& plot_rect, int min_val, int max_val) {
    p.save();
    const auto& pt = history_[hovered_idx_];

    qint64 t_start = history_.front().time_ms;
    qint64 t_end = history_.back().time_ms;
    if (t_end <= t_start) t_end = t_start + 1;

    double ratio = static_cast<double>(pt.time_ms - t_start) / (t_end - t_start);
    int guide_x = plot_rect.left() + static_cast<int>(ratio * plot_rect.width());

    // 垂直引导虚线
    p.setPen(QPen(QColor(56, 139, 253, 160), 1, Qt::DashLine));
    p.drawLine(guide_x, plot_rect.top(), guide_x, plot_rect.bottom());

    auto val_to_y = [&](int val) -> double {
        double r = static_cast<double>(val - min_val) / std::max(1, (max_val - min_val));
        return plot_rect.bottom() - r * plot_rect.height();
    };

    // 绘制曲线交点动态聚焦环
    p.setPen(QPen(QColor("#ffffff"), 1.8));
    p.setBrush(QColor("#388bfd"));
    p.drawEllipse(QPointF(guide_x, val_to_y(pt.total)), 4.0, 4.0);

    // 悬停卡片
    int card_w = 150;
    int card_h = 90;
    int card_x = guide_x + 12;
    int card_y = last_mouse_pos_.y() - card_h / 2;

    if (card_x + card_w > width() - 10) card_x = guide_x - card_w - 12;
    if (card_y < plot_rect.top()) card_y = plot_rect.top() + 4;
    if (card_y + card_h > plot_rect.bottom()) card_y = plot_rect.bottom() - card_h - 4;

    QRect card_rect(card_x, card_y, card_w, card_h);
    p.setPen(QPen(QColor("#388bfd"), 1));
    p.setBrush(QColor(13, 17, 23, 235));
    p.drawRoundedRect(card_rect, 6, 6);

    bool is_veh = ConfigManager::instance().is_vehicle_mode();

    p.setFont(QFont("Segoe UI", 9, QFont::Bold));
    p.setPen(QColor("#f0f6fc"));
    p.drawText(card_x + 10, card_y + 18, QString("🕒 %1").arg(QDateTime::fromMSecsSinceEpoch(pt.time_ms).toString("hh:mm:ss")));

    p.setFont(QFont("Segoe UI", 8));
    p.setPen(QColor("#388bfd"));
    p.drawText(card_x + 10, card_y + 36, QString(is_veh ? "🚗 实时总计: %1 车次" : "👥 实时总计: %1 人次").arg(pt.total));

    p.setPen(QColor("#2ea043"));
    p.drawText(card_x + 10, card_y + 52, QString(is_veh ? "🟢 累计进车: %1 车次" : "🟢 累计进入: %1 人次").arg(pt.entry));

    p.setPen(QColor("#f85149"));
    p.drawText(card_x + 10, card_y + 68, QString(is_veh ? "🔴 累计出车: %1 车次" : "🔴 累计离开: %1 人次").arg(pt.exit));

    int net = pt.entry - pt.exit;
    p.setPen(net >= 0 ? QColor("#3fb950") : QColor("#ffa502"));
    p.drawText(card_x + 10, card_y + 84, QString("📊 净增滞留: %1%2 人").arg(net > 0 ? "+" : "").arg(net));

    p.restore();
}

void RealtimeTotalChartWidget::mouseMoveEvent(QMouseEvent* event) {
    last_mouse_pos_ = event->pos();
    QRect plot_rect(55, 45, width() - 80, height() - 78);

    if (plot_rect.contains(last_mouse_pos_) && history_.size() >= 2) {
        qint64 t_start = history_.front().time_ms;
        qint64 t_end = history_.back().time_ms;
        double ratio = static_cast<double>(last_mouse_pos_.x() - plot_rect.left()) / plot_rect.width();
        ratio = std::clamp(ratio, 0.0, 1.0);
        qint64 target_t = t_start + static_cast<qint64>(ratio * (t_end - t_start));

        int best_idx = 0;
        qint64 min_diff = 999999999;
        for (int i = 0; i < static_cast<int>(history_.size()); ++i) {
            qint64 diff = std::abs(history_[i].time_ms - target_t);
            if (diff < min_diff) {
                min_diff = diff;
                best_idx = i;
            }
        }
        if (hovered_idx_ != best_idx) {
            hovered_idx_ = best_idx;
            update();
        }
    } else if (hovered_idx_ != -1) {
        hovered_idx_ = -1;
        update();
    }
}

void RealtimeTotalChartWidget::leaveEvent(QEvent*) {
    if (hovered_idx_ != -1) {
        hovered_idx_ = -1;
        update();
    }
}
