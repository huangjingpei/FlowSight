#pragma once

#include <QWidget>
#include <map>
#include <utility>
#include <QString>

/**
 * @brief 24 小时各时段客流进出分布高科技矢量图表组件
 * @note  基于 QPainter 硬件双缓冲与抗锯齿渲染，支持双色柱状图、网格刻度与鼠标悬停浮动数据卡片。
 */
class AnalyticsChartWidget : public QWidget {
    Q_OBJECT
public:
    explicit AnalyticsChartWidget(QWidget* parent = nullptr);
    ~AnalyticsChartWidget() override = default;

    // 更新 24 小时数据: hour (0~23) -> {entry_count, exit_count}
    void set_data(const std::map<int, std::pair<int, int>>& hourly_data);
    void clear_data();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void draw_background_and_grid(QPainter& p, const QRect& plot_rect, int max_val);
    void draw_bars_and_axes(QPainter& p, const QRect& plot_rect, int max_val);
    void draw_hover_card(QPainter& p, const QRect& plot_rect);

    std::map<int, std::pair<int, int>> data_;
    int hovered_hour_ = -1;
    QPoint last_mouse_pos_;
};
