#pragma once
#include <QString>

namespace Theme {

inline QString get_global_stylesheet() {
    return R"(
        /* 全局安防工控基础设置 (NVR / DVR 工业级沉浸基底) */
        QWidget {
            background-color: #14161a;
            color: #d1d5db;
            font-family: "Segoe UI", "PingFang SC", "Microsoft YaHei", sans-serif;
            font-size: 12px;
        }

        /* 主窗口与堆栈视图 */
        QMainWindow {
            background-color: #14161a;
        }
        QStackedWidget {
            background-color: #14161a;
            border-left: 1px solid #23272e;
        }

        /* 侧边导航栏 (Standard NVR Navigation Rail) */
        QListWidget#nav_sidebar {
            background-color: #181b20;
            border: none;
            border-right: 1px solid #23272e;
            padding-top: 8px;
            outline: none;
        }
        QListWidget#nav_sidebar::item {
            height: 46px;
            color: #8c96a3;
            padding-left: 14px;
            margin: 2px 6px;
            border-radius: 3px;
            font-size: 12px;
            font-weight: 500;
        }
        QListWidget#nav_sidebar::item:hover {
            background-color: #20242b;
            color: #e6edf3;
        }
        QListWidget#nav_sidebar::item:selected {
            background-color: #222730;
            border-left: 3.5px solid #388bfd;
            color: #ffffff;
            font-weight: bold;
        }

        /* 卡片与分组容器 (GroupBox) */
        QGroupBox {
            background-color: #1a1d22;
            border: 1px solid #282c34;
            border-radius: 4px;
            margin-top: 20px;
            padding: 12px;
            font-weight: bold;
            font-size: 12px;
            color: #58a6ff;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 10px;
            padding: 0 4px;
            background-color: #1a1d22;
        }

        /* 工业级操作按钮 (Buttons) */
        QPushButton {
            background-color: #1f2329;
            color: #c9d1d9;
            border: 1px solid #2d323b;
            border-radius: 3px;
            padding: 5px 14px;
            font-weight: 500;
            font-size: 12px;
        }
        QPushButton:hover {
            background-color: #282c34;
            color: #ffffff;
            border-color: #388bfd;
        }
        QPushButton:pressed {
            background-color: #14161a;
        }
        QPushButton#btn_primary {
            background-color: #238636;
            color: #ffffff;
            border: 1px solid #2ea043;
            font-weight: bold;
        }
        QPushButton#btn_primary:hover {
            background-color: #2ea043;
        }
        QPushButton#btn_accent {
            background-color: #1f6feb;
            color: #ffffff;
            border: 1px solid #388bfd;
            font-weight: bold;
        }
        QPushButton#btn_accent:hover {
            background-color: #388bfd;
        }
        QPushButton#btn_danger {
            background-color: #da3633;
            color: #ffffff;
            border: 1px solid #f85149;
        }
        QPushButton#btn_danger:hover {
            background-color: #f85149;
        }

        /* 输入控件 (LineEdit / ComboBox / SpinBox) */
        QLineEdit, QComboBox, QSpinBox {
            background-color: #16181d;
            color: #c9d1d9;
            border: 1px solid #282c34;
            border-radius: 3px;
            padding: 4px 8px;
            selection-background-color: #1f6feb;
        }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus {
            border: 1px solid #388bfd;
            background-color: #1a1d22;
        }
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 18px;
            border-left: none;
        }

        /* 表格控件 (QTableWidget) */
        QTableWidget {
            background-color: #14161a;
            border: 1px solid #282c34;
            border-radius: 3px;
            gridline-color: #1f2329;
            selection-background-color: #1f6feb22;
            selection-color: #58a6ff;
        }
        QHeaderView::section {
            background-color: #1a1d22;
            color: #8c96a3;
            padding: 6px 8px;
            border: none;
            border-bottom: 1px solid #282c34;
            font-weight: bold;
        }
        QTableWidget::item {
            padding: 4px 8px;
            border-bottom: 1px solid #181b20;
        }
        QTableWidget::item:hover {
            background-color: #1a1d22;
        }

        /* 时间轴与滑动条 (QSlider) */
        QSlider::groove:horizontal {
            height: 4px;
            background: #23272e;
            border-radius: 2px;
        }
        QSlider::sub-page:horizontal {
            background: #388bfd;
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            background: #ffffff;
            width: 12px;
            margin-top: -4px;
            margin-bottom: -4px;
            border-radius: 6px;
            border: 1px solid #2d323b;
        }
        QSlider::handle:horizontal:hover {
            background: #58a6ff;
            border-color: #ffffff;
        }

        /* 滚动区域容器 (QScrollArea) */
        QScrollArea {
            background: transparent;
            border: none;
        }
        QScrollArea > QWidget > QWidget {
            background: transparent;
        }

        /* 极细微安防工控滚动条 (QScrollBar) */
        QScrollBar:vertical {
            background: transparent;
            width: 6px;
            margin: 0px;
            border: none;
        }
        QScrollBar::handle:vertical {
            background: #282c34;
            min-height: 24px;
            border-radius: 3px;
        }
        QScrollBar::handle:vertical:hover {
            background: #388bfd;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
            background: none;
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: none;
        }
        QScrollBar:horizontal {
            height: 0px;
        }

        /* 状态栏 (QStatusBar) */
        QStatusBar {
            background-color: #181b20;
            border-top: 1px solid #23272e;
            color: #8c96a3;
            padding: 2px 8px;
            font-size: 11px;
        }
    )";
}

} // namespace Theme
