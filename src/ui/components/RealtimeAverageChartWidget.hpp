#pragma once

#include <QWidget>
#include <QTimer>
#include <QPushButton>
#include <deque>
#include <cstdint>
#include "common/Types.hpp"

/**
 * @brief 5s / 10s / 30s 客流均值动态时序走势图表
 * @note  支持实时事件滑动窗口采样，动态展示客流均值与流速趋势
 */
class RealtimeAverageChartWidget : public QWidget {
    Q_OBJECT
public:
    explicit RealtimeAverageChartWidget(QWidget* parent = nullptr);
    ~RealtimeAverageChartWidget() override = default;

    void add_passage(const PassageEvent& ev);
    void reset_data();
    void set_window_seconds(int sec);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private slots:
    void on_timer_tick();

private:
    struct AvgDataPoint {
        qint64 time_ms = 0;
        int window_count = 0;     // 窗口内实际通行人次
        float rate_per_min = 0.0f;// 折算每分钟通行速率
        int entry_count = 0;
        int exit_count = 0;
    };

    struct RawEvent {
        qint64 time_ms = 0;
        PassageDirection direction = PassageDirection::ENTRY;
    };

    void setup_controls();
    void recalculate_history();
    void update_button_styles();
    void draw_background_and_grid(QPainter& p, const QRect& plot_rect, float max_rate);
    void draw_curves(QPainter& p, const QRect& plot_rect, float max_rate);
    void draw_hover_card(QPainter& p, const QRect& plot_rect, float max_rate);

    int window_sec_ = 5; // 默认 5s 均值
    int display_span_sec_ = 60; // 滚动图表时间跨度 60 秒

    std::deque<RawEvent> event_log_;
    std::deque<AvgDataPoint> history_;
    QTimer* sample_timer_ = nullptr;

    QPushButton* btn_5s_ = nullptr;
    QPushButton* btn_10s_ = nullptr;
    QPushButton* btn_30s_ = nullptr;

    float peak_rate_ = 0.0f;
    int hovered_idx_ = -1;
    QPoint last_mouse_pos_;
};
