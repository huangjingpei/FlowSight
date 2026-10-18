#pragma once
#include <QString>

namespace Theme {

inline QString get_global_stylesheet() {
    return R"(
        /* 全局基础设置 */
        QWidget {
            background-color: #0d1117;
            color: #e6edf3;
            font-family: "Segoe UI", "PingFang SC", "Microsoft YaHei", sans-serif;
            font-size: 13px;
        }

        /* 主窗口与堆栈视图 */
        QMainWindow {
            background-color: #0d1117;
        }
        QStackedWidget {
            background-color: #0d1117;
            border-left: 1px solid #21262d;
        }

        /* 侧边导航栏 (Navigation Rail) */
        QListWidget#nav_sidebar {
            background-color: #161b22;
            border: none;
            border-right: 1px solid #21262d;
            padding-top: 12px;
            outline: none;
        }
        QListWidget#nav_sidebar::item {
            height: 52px;
            color: #8b949e;
            padding-left: 14px;
            margin: 3px 8px;
            border-radius: 6px;
            font-size: 13px;
            font-weight: 500;
        }
        QListWidget#nav_sidebar::item:hover {
            background-color: #21262d;
            color: #c9d1d9;
        }
        QListWidget#nav_sidebar::item:selected {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #1f6feb, stop:1 #238636);
            color: #ffffff;
            font-weight: bold;
        }

        /* 卡片与分组容器 (GroupBox) */
        QGroupBox {
            background-color: #161b22;
            border: 1px solid #30363d;
            border-radius: 8px;
            margin-top: 24px;
            padding: 14px;
            font-weight: bold;
            font-size: 13px;
            color: #58a6ff;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 12px;
            padding: 0 6px;
            background-color: #161b22;
        }

        /* 现代扁平按钮 (Buttons) */
        QPushButton {
            background-color: #21262d;
            color: #c9d1d9;
            border: 1px solid #30363d;
            border-radius: 6px;
            padding: 7px 16px;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: #30363d;
            color: #f0f6fc;
            border-color: #8b949e;
        }
        QPushButton:pressed {
            background-color: #161b22;
        }
        QPushButton#btn_primary {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #238636, stop:1 #2ea043);
            color: #ffffff;
            border: 1px solid rgba(240, 246, 252, 0.1);
            font-weight: bold;
        }
        QPushButton#btn_primary:hover {
            background: #2ea043;
        }
        QPushButton#btn_accent {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #1f6feb, stop:1 #388bfd);
            color: #ffffff;
            border: 1px solid rgba(240, 246, 252, 0.1);
            font-weight: bold;
        }
        QPushButton#btn_accent:hover {
            background: #388bfd;
        }
        QPushButton#btn_danger {
            background-color: #da3633;
            color: #ffffff;
            border: none;
        }
        QPushButton#btn_danger:hover {
            background-color: #f85149;
        }

        /* 输入控件 (LineEdit / ComboBox / SpinBox) */
        QLineEdit, QComboBox, QSpinBox {
            background-color: #0d1117;
            color: #c9d1d9;
            border: 1px solid #30363d;
            border-radius: 6px;
            padding: 6px 10px;
            selection-background-color: #1f6feb;
        }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus {
            border: 1px solid #58a6ff;
            background-color: #161b22;
        }
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 20px;
            border-left: none;
        }

        /* 表格控件 (QTableWidget) */
        QTableWidget {
            background-color: #0d1117;
            border: 1px solid #30363d;
            border-radius: 6px;
            gridline-color: #21262d;
            selection-background-color: #1f6feb33;
            selection-color: #58a6ff;
        }
        QHeaderView::section {
            background-color: #161b22;
            color: #8b949e;
            padding: 8px 10px;
            border: none;
            border-bottom: 1px solid #30363d;
            font-weight: bold;
        }
        QTableWidget::item {
            padding: 6px 10px;
            border-bottom: 1px solid #161b22;
        }
        QTableWidget::item:hover {
            background-color: #161b22;
        }

        /* 滑动条 (QSlider) */
        QSlider::groove:horizontal {
            height: 4px;
            background: #30363d;
            border-radius: 2px;
        }
        QSlider::sub-page:horizontal {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #1f6feb, stop:1 #58a6ff);
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            background: #58a6ff;
            width: 14px;
            margin-top: -5px;
            margin-bottom: -5px;
            border-radius: 7px;
            border: 2px solid #ffffff;
        }
        QSlider::handle:horizontal:hover {
            background: #79c0ff;
            transform: scale(1.2);
        }

        /* 滚动区域容器 (QScrollArea) */
        QScrollArea {
            background: transparent;
            border: none;
        }
        QScrollArea > QWidget > QWidget {
            background: transparent;
        }

        /* 极细现代科技感滚动条 (QScrollBar) */
        QScrollBar:vertical {
            background: transparent;
            width: 8px;
            margin: 0px;
            border: none;
        }
        QScrollBar::handle:vertical {
            background: #30363d;
            min-height: 28px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical:hover {
            background: #58a6ff;
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
            background-color: #161b22;
            border-top: 1px solid #21262d;
            color: #8b949e;
            padding: 3px 10px;
            font-size: 12px;
        }
    )";
}

} // namespace Theme
