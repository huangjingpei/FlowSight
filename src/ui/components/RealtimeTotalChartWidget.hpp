#pragma once

#include <QWidget>
#include <QTimer>
#include <deque>
#include <cstdint>
#include "common/Types.hpp"

/**
 * @brief 实时的总计人数动态时序曲线图表
 * @note  基于 QPainter 硬件双缓冲与抗锯齿渲染，展示总计人数、累计进入与累计离开的动态滚动走势
 */
class RealtimeTotalChartWidget : public QWidget {
    Q_OBJECT
public:
    explicit RealtimeTotalChartWidget(QWidget* parent = nullptr);
    ~RealtimeTotalChartWidget() override = default;

    void set_counts(int total, int entry, int exit);
    void add_passage(const PassageEvent& ev);
    void reset_data();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private slots:
    void on_timer_tick();

private:
    struct TotalPoint {
        qint64 time_ms = 0;
        int total = 0;
        int entry = 0;
        int exit = 0;
    };

    void draw_background_and_grid(QPainter& p, const QRect& plot_rect, int min_val, int max_val);
    void draw_curves(QPainter& p, const QRect& plot_rect, int min_val, int max_val);
    void draw_hover_card(QPainter& p, const QRect& plot_rect, int min_val, int max_val);

    int current_total_ = 0;
    int current_entry_ = 0;
    int current_exit_ = 0;

    std::deque<TotalPoint> history_;
    QTimer* sample_timer_ = nullptr;

    int hovered_idx_ = -1;
    QPoint last_mouse_pos_;
    int window_span_sec_ = 60; // 滚动展示窗口为 60 秒
};
