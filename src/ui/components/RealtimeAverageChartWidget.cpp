#include "RealtimeAverageChartWidget.hpp"
#include "storage/ConfigManager.hpp"
#include "storage/DatabaseManager.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QDateTime>
#include <QHBoxLayout>
#include <algorithm>
#include <cmath>

RealtimeAverageChartWidget::RealtimeAverageChartWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(320);
    setMouseTracking(true);

    setup_controls();

    // 预热近 120 秒的历史通行事件，保证界面打开即有真实的均值曲线
    qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
    auto recent_evs = DatabaseManager::instance().query_events_by_timerange(now_ms - 120000, now_ms, 500);
    for (const auto& ev : recent_evs) {
        event_log_.push_back({ev.timestamp_ms, ev.direction});
    }

    // 重算初始时序均值
    recalculate_history();

    sample_timer_ = new QTimer(this);
    sample_timer_->setInterval(1000);
    connect(sample_timer_, &QTimer::timeout, this, &RealtimeAverageChartWidget::on_timer_tick);
    sample_timer_->start();
}

void RealtimeAverageChartWidget::setup_controls() {
    // 顶部右侧窗口切换药丸按钮组
    auto* top_layout = new QHBoxLayout();
    top_layout->setContentsMargins(20, 10, 20, 0);
    top_layout->addStretch();

    btn_5s_ = new QPushButton("⚡ 5秒均值", this);
    btn_10s_ = new QPushButton("⏱️ 10秒均值", this);
    btn_30s_ = new QPushButton("🕒 30秒均值", this);

    for (auto* btn : {btn_5s_, btn_10s_, btn_30s_}) {
        btn->setFixedHeight(26);
        btn->setCursor(Qt::PointingHandCursor);
        top_layout->addWidget(btn);
    }

    auto* root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->addLayout(top_layout);
    root_layout->addStretch();

    connect(btn_5s_, &QPushButton::clicked, this, [this]() { set_window_seconds(5); });
    connect(btn_10s_, &QPushButton::clicked, this, [this]() { set_window_seconds(10); });
    connect(btn_30s_, &QPushButton::clicked, this, [this]() { set_window_seconds(30); });

    update_button_styles();
}

void RealtimeAverageChartWidget::update_button_styles() {
    auto set_style = [](QPushButton* btn, bool active) {
        if (active) {
            btn->setStyleSheet(
                "background-color: #388bfd; color: #ffffff; border: 1px solid #58a6ff; "
                "border-radius: 4px; padding: 2px 10px; font-weight: bold; font-size: 11px;"
            );
        } else {
            btn->setStyleSheet(
                "background-color: #21262d; color: #8b949e; border: 1px solid #30363d; "
                "border-radius: 4px; padding: 2px 10px; font-weight: 500; font-size: 11px;"
            );
        }
    };

    set_style(btn_5s_, window_sec_ == 5);
    set_style(btn_10s_, window_sec_ == 10);
    set_style(btn_30s_, window_sec_ == 30);
}

void RealtimeAverageChartWidget::set_window_seconds(int sec) {
    if (window_sec_ == sec) return;
    window_sec_ = sec;
    update_button_styles();
    recalculate_history();
    update();
}

void RealtimeAverageChartWidget::add_passage(const PassageEvent& ev) {
    event_log_.push_back({ev.timestamp_ms > 0 ? ev.timestamp_ms : QDateTime::currentMSecsSinceEpoch(), ev.direction});
    qint64 cutoff_ms = QDateTime::currentMSecsSinceEpoch() - 180000;
    while (!event_log_.empty() && event_log_.front().time_ms < cutoff_ms) {
        event_log_.pop_front();
    }
    recalculate_history(); // 实时即时重算并平滑，提升视觉响应速度
    update();
}

void RealtimeAverageChartWidget::reset_data() {
    event_log_.clear();
    history_.clear();
    peak_rate_ = 0.0f;
    hovered_idx_ = -1;
    recalculate_history();
    update();
}

void RealtimeAverageChartWidget::recalculate_history() {
    history_.clear();
    peak_rate_ = 0.0f;
    qint64 now_ms = QDateTime::currentMSecsSinceEpoch();

    for (int i = display_span_sec_; i >= 0; --i) {
        qint64 t = now_ms - i * 1000;
        qint64 w_start = t - window_sec_ * 1000;

        int w_cnt = 0;
        int w_entry = 0;
        int w_exit = 0;

        for (const auto& ev : event_log_) {
            if (ev.time_ms >= w_start && ev.time_ms <= t) {
                w_cnt++;
                if (ev.direction == PassageDirection::ENTRY) w_entry++;
                else if (ev.direction == PassageDirection::EXIT) w_exit++;
            }
        }

        float rate_min = (static_cast<float>(w_cnt) / window_sec_) * 60.0f;
        peak_rate_ = std::max(peak_rate_, rate_min);
        history_.push_back({t, w_cnt, rate_min, rate_min, w_entry, w_exit});
    }

    // 均值平滑处理：应用 3 点高斯核 (0.20, 0.60, 0.20)，消除离散脉冲方波阶跃感，形成柔和自然的流动波形
    for (size_t i = 0; i < history_.size(); ++i) {
        float p_prev = (i > 0) ? history_[i - 1].rate_per_min : history_[i].rate_per_min;
        float p_curr = history_[i].rate_per_min;
        float p_next = (i + 1 < history_.size()) ? history_[i + 1].rate_per_min : history_[i].rate_per_min;
        history_[i].smooth_rate = 0.20f * p_prev + 0.60f * p_curr + 0.20f * p_next;
    }
}

void RealtimeAverageChartWidget::on_timer_tick() {
    qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
    qint64 w_start = now_ms - window_sec_ * 1000;

    int w_cnt = 0;
    int w_entry = 0;
    int w_exit = 0;

    for (const auto& ev : event_log_) {
        if (ev.time_ms >= w_start && ev.time_ms <= now_ms) {
            w_cnt++;
            if (ev.direction == PassageDirection::ENTRY) w_entry++;
            else if (ev.direction == PassageDirection::EXIT) w_exit++;
        }
    }

    float rate_min = (static_cast<float>(w_cnt) / window_sec_) * 60.0f;
    peak_rate_ = std::max(peak_rate_, rate_min);
    history_.push_back({now_ms, w_cnt, rate_min, rate_min, w_entry, w_exit});

    // 剔除超出显示跨度的数据
    qint64 cutoff_ms = now_ms - display_span_sec_ * 1000;
    while (!history_.empty() && history_.front().time_ms < cutoff_ms) {
        history_.pop_front();
    }

    // 滚动更新均值平滑序列
    for (size_t i = 0; i < history_.size(); ++i) {
        float p_prev = (i > 0) ? history_[i - 1].rate_per_min : history_[i].rate_per_min;
        float p_curr = history_[i].rate_per_min;
        float p_next = (i + 1 < history_.size()) ? history_[i + 1].rate_per_min : history_[i].rate_per_min;
        history_[i].smooth_rate = 0.20f * p_prev + 0.60f * p_curr + 0.20f * p_next;
    }

    // 剔除过老的原始事件记录 (保留3分钟)
    qint64 ev_cutoff = now_ms - 180000;
    while (!event_log_.empty() && event_log_.front().time_ms < ev_cutoff) {
        event_log_.pop_front();
    }

    update();
}

void RealtimeAverageChartWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // 1. 底板容器背景与圆角边框
    QRect full_rect = rect().adjusted(1, 1, -1, -1);
    p.setPen(QPen(QColor("#30363d"), 1));
    p.setBrush(QColor("#161b22"));
    p.drawRoundedRect(full_rect, 8, 8);

    // 2. 顶部标题与当前指标概览
    bool is_veh = ConfigManager::instance().is_vehicle_mode();
    QString title = QString(is_veh ? "⏱️ 车流均值动态走势 (%1秒滑动窗口)" : "⏱️ 客流均值动态走势 (%1秒滑动窗口)").arg(window_sec_);
    p.setFont(QFont("Segoe UI", 11, QFont::Bold));
    p.setPen(QColor("#ffa502"));
    p.drawText(20, 27, title);

    int title_w = p.fontMetrics().horizontalAdvance(title);

    // 当前最新指标
    int cur_cnt = history_.empty() ? 0 : history_.back().window_count;
    float cur_rate = history_.empty() ? 0.0f : history_.back().rate_per_min;

    p.setFont(QFont("Segoe UI", 9));
    int info_x = 20 + title_w + 25;
    p.setPen(QColor("#f0f6fc"));
    p.drawText(info_x, 27, QString(is_veh ? "当前窗口: %1 辆" : "当前窗口: %1 人").arg(cur_cnt));
    info_x += p.fontMetrics().horizontalAdvance(QString(is_veh ? "当前窗口: %1 辆" : "当前窗口: %1 人").arg(cur_cnt)) + 18;

    p.setPen(QColor("#00d2d3"));
    p.drawText(info_x, 27, QString(is_veh ? "实时速率: %1 车次/分" : "实时速率: %1 人次/分").arg(QString::number(cur_rate, 'f', 1)));
    info_x += p.fontMetrics().horizontalAdvance(QString(is_veh ? "实时速率: %1 车次/分" : "实时速率: %1 人次/分").arg(QString::number(cur_rate, 'f', 1))) + 18;

    p.setPen(QColor("#ffa502"));
    p.drawText(info_x, 27, QString(is_veh ? "峰值速率: %1 车次/分" : "峰值速率: %1 人次/分").arg(QString::number(peak_rate_, 'f', 1)));

    // 3. 计算坐标范围
    float max_rate = 12.0f;
    for (const auto& pt : history_) {
        max_rate = std::max(max_rate, pt.rate_per_min);
    }
    // 向上规整到 6 的倍数
    max_rate = std::ceil(max_rate / 6.0f) * 6.0f;

    QRect plot_rect(55, 48, width() - 80, height() - 82);
    if (plot_rect.width() <= 10 || plot_rect.height() <= 10) return;

    // 4. 绘制网格与刻度
    draw_background_and_grid(p, plot_rect, max_rate);

    // 5. 绘制均值平滑曲线与阴影
    draw_curves(p, plot_rect, max_rate);

    // 6. 悬停卡片
    if (hovered_idx_ >= 0 && hovered_idx_ < static_cast<int>(history_.size())) {
        draw_hover_card(p, plot_rect, max_rate);
    }
}

void RealtimeAverageChartWidget::draw_background_and_grid(QPainter& p, const QRect& plot_rect, float max_rate) {
    p.save();
    QPen grid_pen(QColor("#21262d"), 1, Qt::DashLine);
    p.setFont(QFont("Consolas", 8));

    // Y 轴刻度 (4 分格)
    int grid_lines = 4;
    for (int i = 0; i <= grid_lines; ++i) {
        int y = plot_rect.bottom() - (plot_rect.height() * i) / grid_lines;
        float val = (max_rate * i) / grid_lines;

        p.setPen(grid_pen);
        p.drawLine(plot_rect.left(), y, plot_rect.right(), y);

        p.setPen(QColor("#6e7681"));
        p.drawText(QRect(0, y - 8, plot_rect.left() - 8, 16), Qt::AlignRight | Qt::AlignVCenter, QString::number(val, 'f', 0));
    }

    // X 轴时间刻度
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
    // 保证局部极值处一阶导数连续且切线水平(无过冲无振荡)，平坦段与基线永不下冲为负数
    void build_monotone_cubic_path(const std::vector<QPointF>& points,
                                   double baseline_y,
                                   QPainterPath& path_curve,
                                   QPainterPath& path_fill,
                                   double min_y, double max_y) {
        if (points.empty()) return;

        path_curve.moveTo(points.front());
        path_fill.moveTo(points.front().x(), baseline_y);
        path_fill.lineTo(points.front());

        if (points.size() == 1) {
            path_fill.lineTo(points.front().x(), baseline_y);
            path_fill.closeSubpath();
            return;
        }

        if (points.size() == 2) {
            path_curve.lineTo(points.back());
            path_fill.lineTo(points.back());
            path_fill.lineTo(points.back().x(), baseline_y);
            path_fill.closeSubpath();
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
                // 极值点切线置为水平，消除波峰与波谷处的过冲振铃效应
                tangents[i] = 0.0;
            } else {
                tangents[i] = (m[i - 1] + m[i]) / 2.0;
            }
        }

        // Fritsch-Carlson 条件调整
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

        // 构造光滑三次贝塞尔曲线段
        for (size_t i = 0; i < n - 1; ++i) {
            double h = dx[i];
            QPointF p1 = points[i];
            QPointF p2 = points[i + 1];
            QPointF c1(p1.x() + h / 3.0, p1.y() + tangents[i] * h / 3.0);
            QPointF c2(p2.x() - h / 3.0, p2.y() - tangents[i + 1] * h / 3.0);

            // 限制在图表区域内，防止边缘毛刺
            c1.setY(std::clamp(c1.y(), min_y, max_y));
            c2.setY(std::clamp(c2.y(), min_y, max_y));

            path_curve.cubicTo(c1, c2, p2);
            path_fill.cubicTo(c1, c2, p2);
        }

        path_fill.lineTo(points.back().x(), baseline_y);
        path_fill.closeSubpath();
    }
}

void RealtimeAverageChartWidget::draw_curves(QPainter& p, const QRect& plot_rect, float max_rate) {
    if (history_.size() < 2) return;

    p.save();
    qint64 t_start = history_.front().time_ms;
    qint64 t_end = history_.back().time_ms;
    if (t_end <= t_start) t_end = t_start + 1;

    auto rate_to_y = [&](float rate) -> double {
        double ratio = static_cast<double>(rate) / std::max(1.0f, max_rate);
        return plot_rect.bottom() - ratio * plot_rect.height();
    };

    auto time_to_x = [&](qint64 t) -> double {
        double ratio = static_cast<double>(t - t_start) / (t_end - t_start);
        return plot_rect.left() + ratio * plot_rect.width();
    };

    // 收集所有平滑走势采样点
    std::vector<QPointF> points;
    points.reserve(history_.size());
    for (const auto& pt : history_) {
        points.emplace_back(time_to_x(pt.time_ms), rate_to_y(pt.smooth_rate));
    }

    QPainterPath path_curve;
    QPainterPath path_fill;
    build_monotone_cubic_path(points, plot_rect.bottom(), path_curve, path_fill, plot_rect.top(), plot_rect.bottom());

    // 1. 区域平滑渐变辉光填充 (多阶温暖琥珀金渐变)
    QLinearGradient grad(QPointF(0, plot_rect.top()), QPointF(0, plot_rect.bottom()));
    grad.setColorAt(0.0, QColor(255, 165, 2, 75));
    grad.setColorAt(0.4, QColor(255, 165, 2, 28));
    grad.setColorAt(1.0, QColor(255, 165, 2, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(grad);
    p.drawPath(path_fill);

    // 2. 绘制平滑均值贝塞尔主曲线 (抗锯齿圆角描边)
    QPen curve_pen(QColor("#ffa502"), 2.5);
    curve_pen.setCapStyle(Qt::RoundCap);
    curve_pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(curve_pen);
    p.setBrush(Qt::NoBrush);
    p.drawPath(path_curve);

    // 3. 最新实时点发光环 (呼吸光圈与高光核)
    if (!points.empty()) {
        const QPointF& latest = points.back();
        // 外层微光光晕
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 165, 2, 45));
        p.drawEllipse(latest, 7.5, 7.5);
        // 主光环
        p.setBrush(QColor(255, 165, 2, 160));
        p.drawEllipse(latest, 4.2, 4.2);
        // 核心白亮光点
        p.setBrush(QColor("#ffffff"));
        p.drawEllipse(latest, 2.0, 2.0);
    }

    p.restore();
}

void RealtimeAverageChartWidget::draw_hover_card(QPainter& p, const QRect& plot_rect, float max_rate) {
    p.save();
    const auto& pt = history_[hovered_idx_];

    qint64 t_start = history_.front().time_ms;
    qint64 t_end = history_.back().time_ms;
    if (t_end <= t_start) t_end = t_start + 1;

    double ratio = static_cast<double>(pt.time_ms - t_start) / (t_end - t_start);
    int guide_x = plot_rect.left() + static_cast<int>(ratio * plot_rect.width());

    auto rate_to_y = [&](float rate) -> double {
        double r = static_cast<double>(rate) / std::max(1.0f, max_rate);
        return plot_rect.bottom() - r * plot_rect.height();
    };

    double cur_y = rate_to_y(pt.smooth_rate);

    // 垂直引导虚线
    p.setPen(QPen(QColor(255, 165, 2, 140), 1, Qt::DashLine));
    p.drawLine(guide_x, plot_rect.top(), guide_x, plot_rect.bottom());

    // 曲线交叉点动态聚焦环
    p.setPen(QPen(QColor("#ffffff"), 1.8));
    p.setBrush(QColor("#ffa502"));
    p.drawEllipse(QPointF(guide_x, cur_y), 4.5, 4.5);

    // 悬停卡片
    int card_w = 168;
    int card_h = 88;
    int card_x = guide_x + 12;
    int card_y = last_mouse_pos_.y() - card_h / 2;

    if (card_x + card_w > width() - 10) card_x = guide_x - card_w - 12;
    if (card_y < plot_rect.top()) card_y = plot_rect.top() + 4;
    if (card_y + card_h > plot_rect.bottom()) card_y = plot_rect.bottom() - card_h - 4;

    QRect card_rect(card_x, card_y, card_w, card_h);
    p.setPen(QPen(QColor("#ffa502"), 1));
    p.setBrush(QColor(13, 17, 23, 238));
    p.drawRoundedRect(card_rect, 6, 6);

    p.setFont(QFont("Segoe UI", 9, QFont::Bold));
    p.setPen(QColor("#f0f6fc"));
    p.drawText(card_x + 10, card_y + 18, QString("🕒 %1 (采样 %2s)").arg(QDateTime::fromMSecsSinceEpoch(pt.time_ms).toString("hh:mm:ss")).arg(window_sec_));

    bool is_veh = ConfigManager::instance().is_vehicle_mode();
    p.setFont(QFont("Segoe UI", 8));
    p.setPen(QColor("#ffa502"));
    p.drawText(card_x + 10, card_y + 36, QString(is_veh ? "🚗 窗口通行: %1 辆 (进%2/出%3)" : "👥 窗口通行: %1 人 (进%2/出%3)").arg(pt.window_count).arg(pt.entry_count).arg(pt.exit_count));

    p.setPen(QColor("#00d2d3"));
    p.drawText(card_x + 10, card_y + 53, QString(is_veh ? "⚡ 折算速率: %1 车次/分" : "⚡ 折算速率: %1 人次/分").arg(QString::number(pt.rate_per_min, 'f', 1)));

    float r_sec = pt.rate_per_min / 60.0f;
    p.setPen(QColor("#a29bfe"));
    p.drawText(card_x + 10, card_y + 70, QString(is_veh ? "📊 瞬时密度: %1 车/秒" : "📊 瞬时密度: %1 人/秒").arg(QString::number(r_sec, 'f', 2)));

    p.restore();
}

void RealtimeAverageChartWidget::mouseMoveEvent(QMouseEvent* event) {
    last_mouse_pos_ = event->pos();
    QRect plot_rect(55, 48, width() - 80, height() - 82);

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

void RealtimeAverageChartWidget::leaveEvent(QEvent*) {
    if (hovered_idx_ != -1) {
        hovered_idx_ = -1;
        update();
    }
}
