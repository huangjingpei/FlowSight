#include "AnalyticsChartWidget.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <algorithm>

AnalyticsChartWidget::AnalyticsChartWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(260);
    setMouseTracking(true);
}

void AnalyticsChartWidget::set_data(const std::map<int, std::pair<int, int>>& hourly_data) {
    data_ = hourly_data;
    update();
}

void AnalyticsChartWidget::clear_data() {
    data_.clear();
    hovered_hour_ = -1;
    update();
}

void AnalyticsChartWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // 1. 底板背景与圆角边框
    QRect full_rect = rect().adjusted(1, 1, -1, -1);
    p.setPen(QPen(QColor("#30363d"), 1));
    p.setBrush(QColor("#161b22"));
    p.drawRoundedRect(full_rect, 8, 8);

    // 2. 顶部标题与图例
    p.setFont(QFont("Segoe UI", 11, QFont::Bold));
    p.setPen(QColor("#58a6ff"));
    p.drawText(20, 26, "📊 24小时各时段进出流量时序分布 (矢量图形化看板)");

    // 图例: 绿色进入、红色离开
    p.setFont(QFont("Segoe UI", 9));
    int legend_x = width() - 200;
    // 进入
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#00d2d3"));
    p.drawRoundedRect(legend_x, 16, 12, 12, 2, 2);
    p.setPen(QColor("#c9d1d9"));
    p.drawText(legend_x + 18, 27, "进入 (IN)");

    // 离开
    legend_x += 90;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#ff4757"));
    p.drawRoundedRect(legend_x, 16, 12, 12, 2, 2);
    p.setPen(QColor("#c9d1d9"));
    p.drawText(legend_x + 18, 27, "离开 (OUT)");

    // 3. 计算数据峰值与绘图区域
    int max_val = 5;
    int total_sum = 0;
    for (int h = 0; h < 24; ++h) {
        auto it = data_.find(h);
        if (it != data_.end()) {
            max_val = std::max(max_val, it->second.first);
            max_val = std::max(max_val, it->second.second);
            total_sum += (it->second.first + it->second.second);
        }
    }
    // 向上规整到 5 的倍数
    max_val = ((max_val + 4) / 5) * 5;

    QRect plot_rect(50, 48, width() - 75, height() - 85);

    if (total_sum == 0) {
        draw_background_and_grid(p, plot_rect, max_val);
        p.setFont(QFont("Segoe UI", 12));
        p.setPen(QColor("#8b949e"));
        p.drawText(plot_rect, Qt::AlignCenter, "暂无当前时间范围内的通行数据\n请在主监控加载视频并触发进出事件");
        return;
    }

    // 4. 绘制网格与坐标轴
    draw_background_and_grid(p, plot_rect, max_val);

    // 5. 绘制 24 小时双色柱状图
    draw_bars_and_axes(p, plot_rect, max_val);

    // 6. 绘制悬停浮动数据卡片
    if (hovered_hour_ >= 0 && hovered_hour_ < 24) {
        draw_hover_card(p, plot_rect);
    }
}

void AnalyticsChartWidget::draw_background_and_grid(QPainter& p, const QRect& plot_rect, int max_val) {
    p.save();
    QPen grid_pen(QColor("#21262d"), 1, Qt::DashLine);
    p.setFont(QFont("Consolas", 8));

    int grid_lines = 4;
    for (int i = 0; i <= grid_lines; ++i) {
        int y = plot_rect.bottom() - (plot_rect.height() * i) / grid_lines;
        int val = (max_val * i) / grid_lines;

        p.setPen(grid_pen);
        p.drawLine(plot_rect.left(), y, plot_rect.right(), y);

        p.setPen(QColor("#6e7681"));
        p.drawText(QRect(0, y - 8, plot_rect.left() - 8, 16), Qt::AlignRight | Qt::AlignVCenter, QString::number(val));
    }
    p.restore();
}

void AnalyticsChartWidget::draw_bars_and_axes(QPainter& p, const QRect& plot_rect, int max_val) {
    p.save();
    double hour_w = static_cast<double>(plot_rect.width()) / 24.0;
    double bar_w = std::max(2.0, (hour_w - 6.0) / 2.0);

    for (int h = 0; h < 24; ++h) {
        double cell_left = plot_rect.left() + h * hour_w;

        // X 轴时段刻度 (每隔 2 或 3 小时绘制一次或整点绘制)
        if (h % 2 == 0) {
            p.setFont(QFont("Consolas", 8));
            p.setPen(QColor("#8b949e"));
            p.drawText(QRect(static_cast<int>(cell_left - 10), plot_rect.bottom() + 6, static_cast<int>(hour_w + 20), 18),
                       Qt::AlignCenter, QString("%1:00").arg(h, 2, 10, QChar('0')));
        }

        int entry_cnt = 0;
        int exit_cnt = 0;
        auto it = data_.find(h);
        if (it != data_.end()) {
            entry_cnt = it->second.first;
            exit_cnt = it->second.second;
        }

        // 高亮当前悬停时段底色
        if (h == hovered_hour_) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(88, 166, 255, 30));
            p.drawRoundedRect(QRectF(cell_left + 1, plot_rect.top(), hour_w - 2, plot_rect.height()), 4, 4);
        }

        // 绘制进入柱 (Cyan)
        if (entry_cnt > 0) {
            double h_px = (static_cast<double>(entry_cnt) / max_val) * plot_rect.height();
            QRectF bar_rect(cell_left + 2, plot_rect.bottom() - h_px, bar_w, h_px);

            QLinearGradient grad(bar_rect.topLeft(), bar_rect.bottomLeft());
            grad.setColorAt(0.0, QColor("#00d2d3"));
            grad.setColorAt(1.0, QColor("#10ac84"));

            p.setPen(Qt::NoPen);
            p.setBrush(grad);
            p.drawRoundedRect(bar_rect, 2, 2);
        }

        // 绘制离开柱 (Coral)
        if (exit_cnt > 0) {
            double h_px = (static_cast<double>(exit_cnt) / max_val) * plot_rect.height();
            QRectF bar_rect(cell_left + 3 + bar_w, plot_rect.bottom() - h_px, bar_w, h_px);

            QLinearGradient grad(bar_rect.topLeft(), bar_rect.bottomLeft());
            grad.setColorAt(0.0, QColor("#ff4757"));
            grad.setColorAt(1.0, QColor("#ee5253"));

            p.setPen(Qt::NoPen);
            p.setBrush(grad);
            p.drawRoundedRect(bar_rect, 2, 2);
        }
    }
    p.restore();
}

void AnalyticsChartWidget::draw_hover_card(QPainter& p, const QRect& plot_rect) {
    p.save();
    int entry_cnt = 0, exit_cnt = 0;
    auto it = data_.find(hovered_hour_);
    if (it != data_.end()) {
        entry_cnt = it->second.first;
        exit_cnt = it->second.second;
    }

    int card_w = 150;
    int card_h = 76;
    int card_x = last_mouse_pos_.x() + 15;
    int card_y = last_mouse_pos_.y() - card_h - 10;

    if (card_x + card_w > width() - 10) card_x = last_mouse_pos_.x() - card_w - 15;
    if (card_y < plot_rect.top()) card_y = last_mouse_pos_.y() + 15;

    QRect card_rect(card_x, card_y, card_w, card_h);

    // 半透明毛玻璃卡片
    p.setPen(QPen(QColor("#58a6ff"), 1));
    p.setBrush(QColor(13, 17, 23, 235));
    p.drawRoundedRect(card_rect, 6, 6);

    // 内部文字排版
    p.setFont(QFont("Segoe UI", 9, QFont::Bold));
    p.setPen(QColor("#f0f6fc"));
    p.drawText(card_x + 10, card_y + 18, QString("🕒 时段 %1:00 ~ %2:00")
        .arg(hovered_hour_, 2, 10, QChar('0'))
        .arg((hovered_hour_ + 1) % 24, 2, 10, QChar('0')));

    p.setFont(QFont("Segoe UI", 8));
    p.setPen(QColor("#00d2d3"));
    p.drawText(card_x + 10, card_y + 36, QString("🟢 进入: %1 人次").arg(entry_cnt));

    p.setPen(QColor("#ff4757"));
    p.drawText(card_x + 10, card_y + 52, QString("🔴 离开: %1 人次").arg(exit_cnt));

    int net = entry_cnt - exit_cnt;
    p.setPen(net >= 0 ? QColor("#3fb950") : QColor("#ffa502"));
    p.drawText(card_x + 10, card_y + 68, QString("📊 净增: %1%2 人次").arg(net > 0 ? "+" : "").arg(net));

    p.restore();
}

void AnalyticsChartWidget::mouseMoveEvent(QMouseEvent* event) {
    last_mouse_pos_ = event->pos();
    QRect plot_rect(50, 48, width() - 75, height() - 85);

    if (plot_rect.contains(last_mouse_pos_)) {
        double hour_w = static_cast<double>(plot_rect.width()) / 24.0;
        int h = static_cast<int>((last_mouse_pos_.x() - plot_rect.left()) / hour_w);
        h = std::clamp(h, 0, 23);
        if (h != hovered_hour_) {
            hovered_hour_ = h;
            update();
        }
    } else if (hovered_hour_ != -1) {
        hovered_hour_ = -1;
        update();
    }
}

void AnalyticsChartWidget::leaveEvent(QEvent*) {
    if (hovered_hour_ != -1) {
        hovered_hour_ = -1;
        update();
    }
}
