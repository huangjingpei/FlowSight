# FlowSight 客流统计系统 — MVP 落地实施指南与代码骨架

- **文档版本**：v1.2（多页面商业架构演进版）
- **配套文档**：[`00-FlowSight_MVP_技术原型.md`](file:///e:/FlowSight/docs/00-FlowSight_MVP_技术原型.md)
- **目标**：提供**Qt 多页面工作区路由（QStackedWidget + 侧边栏导航）、任意 N 边形标定画布、YOLOv8n 推理、ByteTrack 跟踪、短视频切片与 SQLite 存盘**的完整工程代码骨架。

---

## 1. 工程目录结构规划 (多页面解耦版)

```text
FlowSight/
├── CMakeLists.txt                 # 顶层 CMake 构建脚本
├── docs/                          # 规格与实施手册
│   ├── 00-FlowSight_MVP_技术原型.md
│   ├── 01-FlowSight_MVP_落地实施指南与代码骨架.md
│   └── 02-FlowSight_MVP_全景操作与部署手册.md
├── configs/
│   └── app_config.json
├── models/
│   └── yolov8n.onnx
├── clips/                         # 关键通行短视频自动切片目录
├── 3rdparty/
│   └── bytetrack/                 # ByteTrack C++ 轻量库源码
└── src/
    ├── common/                    # 公共类型与队列
    │   ├── Types.hpp
    │   └── ThreadSafeQueue.hpp
    ├── ai/                        # 检测器模块 (IDetector 抽象)
    │   ├── IDetector.hpp
    │   ├── OnnxDetector.hpp
    │   └── OnnxDetector.cpp
    ├── tracking/                  # 跟踪适配器
    │   ├── ITracker.hpp
    │   ├── ByteTrackWrapper.hpp
    │   └── ByteTrackWrapper.cpp
    ├── analytics/                 # 区域判定与滞后状态机
    │   ├── RoiCounter.hpp
    │   └── RoiCounter.cpp
    ├── storage/                   # SQLite WAL 存储
    │   ├── DatabaseManager.hpp
    │   └── DatabaseManager.cpp
    ├── video/                     # 采集与短视频切片录制
    │   ├── VideoCaptureWorker.hpp
    │   ├── VideoCaptureWorker.cpp
    │   └── ClipRecorder.hpp
    ├── pipeline/                  # 流水线控制器
    │   ├── PipelineController.hpp
    │   └── PipelineController.cpp
    └── ui/                        # Qt 现代化多页面界面
        ├── VideoCanvas.hpp        # 核心多边形绘制与视频画布
        ├── VideoCanvas.cpp
        ├── views/                 # 5 大独立工作区视图
        │   ├── MonitorWidget.hpp   # 页面 1: 实时监控中心 (只读防误触)
        │   ├── MonitorWidget.cpp
        │   ├── StudioWidget.hpp    # 页面 2: 空间标定工作台 (N 边形编辑)
        │   ├── StudioWidget.cpp
        │   ├── EvidenceWidget.hpp  # 页面 3: 事件与视频回溯中心 (切片播放)
        │   ├── EvidenceWidget.cpp
        │   ├── AnalyticsWidget.hpp # 页面 4: 统计报表看板 (趋势柱状图)
        │   ├── AnalyticsWidget.cpp
        │   ├── SettingsWidget.hpp  # 页面 5: 系统与存储设置
        │   └── SettingsWidget.cpp
        ├── MainWindow.hpp         # 侧边导航与路由总控
        ├── MainWindow.cpp
        └── main.cpp
```

---

## 2. 根目录 `CMakeLists.txt` 构建模板

```cmake
cmake_minimum_required(VERSION 3.24)
project(FlowSight VERSION 1.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 1. 查找 Qt6 组件 (含 Multimedia 模块支撑短视频切片播放)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Sql Multimedia MultimediaWidgets)

# 2. 查找 OpenCV
find_package(OpenCV REQUIRED)

# 3. 配置 ONNX Runtime GPU SDK
set(ONNXRUNTIME_ROOT_DIR "" CACHE PATH "Path to ONNX Runtime GPU SDK")
if(NOT ONNXRUNTIME_ROOT_DIR)
    message(FATAL_ERROR "Please specify ONNXRUNTIME_ROOT_DIR!")
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty
    ${OpenCV_INCLUDE_DIRS}
    ${ONNXRUNTIME_ROOT_DIR}/include
)

link_directories(
    ${ONNXRUNTIME_ROOT_DIR}/lib
)

# 4. 收集所有模块源码
file(GLOB_RECURSE SRC_COMMON "src/common/*.hpp")
file(GLOB_RECURSE SRC_AI "src/ai/*.cpp" "src/ai/*.hpp")
file(GLOB_RECURSE SRC_TRACKING "src/tracking/*.cpp" "src/tracking/*.hpp" "3rdparty/bytetrack/*.cpp" "3rdparty/bytetrack/*.h")
file(GLOB_RECURSE SRC_ANALYTICS "src/analytics/*.cpp" "src/analytics/*.hpp")
file(GLOB_RECURSE SRC_STORAGE "src/storage/*.cpp" "src/storage/*.hpp")
file(GLOB_RECURSE SRC_VIDEO "src/video/*.cpp" "src/video/*.hpp")
file(GLOB_RECURSE SRC_PIPELINE "src/pipeline/*.cpp" "src/pipeline/*.hpp")
file(GLOB_RECURSE SRC_UI "src/ui/*.cpp" "src/ui/*.hpp" "src/ui/views/*.cpp" "src/ui/views/*.hpp")

# 5. 可执行文件
add_executable(FlowSight
    src/ui/main.cpp
    ${SRC_COMMON}
    ${SRC_AI}
    ${SRC_TRACKING}
    ${SRC_ANALYTICS}
    ${SRC_STORAGE}
    ${SRC_VIDEO}
    ${SRC_PIPELINE}
    ${SRC_UI}
)

# 6. 链接库
target_link_libraries(FlowSight PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    ${OpenCV_LIBS}
    onnxruntime
)

# 7. Windows 运行时 DLL 自动拷贝
if(WIN32)
    add_custom_command(TARGET FlowSight POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ONNXRUNTIME_ROOT_DIR}/lib/onnxruntime.dll"
        $<TARGET_FILE_DIR:FlowSight>
    )
    add_custom_command(TARGET FlowSight POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ONNXRUNTIME_ROOT_DIR}/lib/onnxruntime_providers_cuda.dll"
        $<TARGET_FILE_DIR:FlowSight>
    )
endif()
```

---

## 3. 核心数据类型契约 (`src/common/Types.hpp`)

```cpp
#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <opencv2/core.hpp>

// 检测框结构
struct Detection {
    cv::Rect2f bbox;        // 像素坐标 [x, y, w, h]
    float confidence;      // 置信度
    int class_id;          // 目标类别 (0: person)
};

// 跟踪轨迹结构（含预留属性字段）
struct TrackObject {
    int track_id;          // 跟踪 ID
    cv::Rect2f bbox;       // 目标框
    cv::Point2f foot_point;// 足底接触代表点
    float score;           // 置信度
    int track_len;         // 存活帧数
    
    // 预留基础客流属性
    std::string gender = "未知";     // "男" / "女" / "未知"
    std::string age_group = "未知";  // "青年(20-30)" / "中年(30-50)" / "未知"
};

// 任意 N 边形 ROI 配置
struct RoiConfig {
    int version = 0;
    std::string source_id = "default";
    int ref_width = 1920;
    int ref_height = 1080;
    std::vector<cv::Point2f> vertices; // 任意 N 边形原始像素顶点 (N >= 3)
    float buffer_distance = 20.0f;     // 几何缓冲带距离 (px)
    int min_confirm_frames = 4;        // 状态确认帧数
    bool is_valid = false;
};

// 进出事件类型
enum class PassageDirection {
    ENTRY = 1,
    EXIT = 2
};

// 通行事实明细（含短视频切片路径）
struct PassageEvent {
    int64_t timestamp_ms;
    int track_id;
    PassageDirection direction;
    int roi_version;
    float confidence;
    std::string source_id;
    std::string clip_path;   // 关联的短视频切片路径 (如 "clips/clip_12.mp4")
    std::string gender;
    std::string age_group;
};
```

---

## 4. 多页面总控路由 (`src/ui/MainWindow.hpp` & `.cpp`)

```cpp
// src/ui/MainWindow.hpp
#pragma once
#include <QMainWindow>
#include <QStackedWidget>
#include <QListWidget>
#include <QLabel>
#include <memory>
#include "pipeline/PipelineController.hpp"

class MonitorWidget;
class StudioWidget;
class EvidenceWidget;
class AnalyticsWidget;
class SettingsWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_nav_item_changed(int row);
    void on_roi_calibrated(const RoiConfig& new_roi);

private:
    void setup_ui();
    void setup_connections();

    QListWidget* nav_sidebar_ = nullptr;
    QStackedWidget* stacked_views_ = nullptr;

    MonitorWidget* view_monitor_ = nullptr;
    StudioWidget* view_studio_ = nullptr;
    EvidenceWidget* view_evidence_ = nullptr;
    AnalyticsWidget* view_analytics_ = nullptr;
    SettingsWidget* view_settings_ = nullptr;

    QLabel* status_infer_label_ = nullptr;
    QLabel* status_fps_label_ = nullptr;

    std::unique_ptr<PipelineController> pipeline_;
};
```

```cpp
// src/ui/MainWindow.cpp
#include "MainWindow.hpp"
#include "views/MonitorWidget.hpp"
#include "views/StudioWidget.hpp"
#include "views/EvidenceWidget.hpp"
#include "views/AnalyticsWidget.hpp"
#include "views/SettingsWidget.hpp"

#include <QHBoxLayout>
#include <QStatusBar>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("FlowSight 智能客流分析系统 v1.2");
    resize(1280, 768);
    setMinimumSize(1024, 600);

    setup_ui();
    setup_connections();

    pipeline_ = std::make_unique<PipelineController>();
    pipeline_->start_pipeline("models/yolov8n.onnx", "configs/test_video.mp4");
}

MainWindow::~MainWindow() = default;

void MainWindow::setup_ui() {
    auto* central_widget = new QWidget(this);
    auto* main_layout = new QHBoxLayout(central_widget);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);

    // 1. 左侧现代化导航栏
    nav_sidebar_ = new QListWidget(this);
    nav_sidebar_->setFixedWidth(120);
    nav_sidebar_->setViewMode(QListView::IconMode);
    nav_sidebar_->setMovement(QListView::Static);
    nav_sidebar_->setSpacing(10);
    nav_sidebar_->setIconSize(QSize(28, 28));

    nav_sidebar_->addItem("实时监控");
    nav_sidebar_->addItem("空间标定");
    nav_sidebar_->addItem("证据回溯");
    nav_sidebar_->addItem("数据报表");
    nav_sidebar_->addItem("系统设置");
    nav_sidebar_->setCurrentRow(0);

    // 2. 右侧多页面堆栈容器
    stacked_views_ = new QStackedWidget(this);
    view_monitor_ = new MonitorWidget(this);
    view_studio_ = new StudioWidget(this);
    view_evidence_ = new EvidenceWidget(this);
    view_analytics_ = new AnalyticsWidget(this);
    view_settings_ = new SettingsWidget(this);

    stacked_views_->addWidget(view_monitor_);    // Index 0
    stacked_views_->addWidget(view_studio_);     // Index 1
    stacked_views_->addWidget(view_evidence_);   // Index 2
    stacked_views_->addWidget(view_analytics_);  // Index 3
    stacked_views_->addWidget(view_settings_);   // Index 4

    main_layout->addWidget(nav_sidebar_);
    main_layout->addWidget(stacked_views_);
    setCentralWidget(central_widget);

    // 3. 底部状态栏
    status_infer_label_ = new QLabel("推理: CUDA 14.2 ms", this);
    status_fps_label_ = new QLabel("帧率: 29.8 FPS", this);
    statusBar()->addWidget(new QLabel("设备: 正常运行 | ", this));
    statusBar()->addWidget(status_infer_label_);
    statusBar()->addWidget(new QLabel(" | ", this));
    statusBar()->addWidget(status_fps_label_);
}

void MainWindow::setup_connections() {
    connect(nav_sidebar_, &QListWidget::currentRowChanged, this, &MainWindow::on_nav_item_changed);
    connect(view_studio_, &StudioWidget::roi_saved, this, &MainWindow::on_roi_calibrated);
}

void MainWindow::on_nav_item_changed(int row) {
    stacked_views_->setCurrentIndex(row);
}

void MainWindow::on_roi_calibrated(const RoiConfig& new_roi) {
    if (pipeline_) {
        pipeline_->update_roi_config(new_roi);
    }
    view_monitor_->update_active_roi(new_roi);
}
```

---

## 5. 任意 N 边形交互画布 (`src/ui/VideoCanvas.cpp`)

支持**单击加点、双击闭合 N 边形、顶点拖拽及整体平移**。

```cpp
#include "VideoCanvas.hpp"
#include <QPainter>
#include <QMouseEvent>
#include <opencv2/imgproc.hpp>
#include <algorithm>

VideoCanvas::VideoCanvas(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setMinimumSize(640, 360);
}

void VideoCanvas::update_frame(const QImage& frame, const std::vector<TrackObject>& tracks) {
    current_frame_ = frame;
    current_tracks_ = tracks;
    update();
}

void VideoCanvas::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    if (current_frame_.isNull()) return;

    // 1. 计算 Letterbox
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

    // 3. 绘制 ROI
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
            painter.setBrush(QColor(255, 215, 0, 60));
            painter.setPen(QPen(QColor(255, 215, 0, 220), 2, Qt::SolidLine));
            painter.drawPolygon(poly);

            if (mode_ == EditMode::EDITING) {
                painter.setBrush(Qt::white);
                painter.setPen(QPen(Qt::red, 2));
                for (int i = 0; i < poly.size(); ++i) {
                    painter.drawEllipse(poly[i], 6, 6);
                }
            }
        }
    }

    // 4. 绘制检测目标框与属性
    painter.setBrush(Qt::NoBrush);
    for (const auto& trk : current_tracks_) {
        cv::Point2f top_left(trk.bbox.x, trk.bbox.y);
        QPointF c_tl = frame_to_canvas(top_left);
        float w = trk.bbox.width * scale_;
        float h = trk.bbox.height * scale_;

        painter.setPen(QPen(Qt::green, 2));
        painter.drawRect(QRectF(c_tl.x(), c_tl.y(), w, h));

        QPointF c_foot = frame_to_canvas(trk.foot_point);
        painter.setBrush(Qt::red);
        painter.drawEllipse(c_foot, 4, 4);

        painter.setPen(Qt::white);
        QString tag = QString("ID:%1 [%2|%3]")
                          .arg(trk.track_id)
                          .arg(QString::fromStdString(trk.gender))
                          .arg(QString::fromStdString(trk.age_group));
        painter.drawText(QPointF(c_tl.x(), c_tl.y() - 4), tag);
        painter.setBrush(Qt::NoBrush);
    }
}

void VideoCanvas::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton || !is_point_in_video_rect(event->position())) {
        return;
    }

    QPointF mouse_pos = event->position();
    last_mouse_pos_ = mouse_pos;

    if (mode_ == EditMode::DRAWING) {
        editing_roi_.vertices.push_back(canvas_to_frame(mouse_pos));
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
    if (mode_ == EditMode::DRAWING && editing_roi_.vertices.size() >= 3) {
        mode_ = EditMode::EDITING;
        editing_roi_.is_valid = validate_polygon(editing_roi_.vertices);
        update();
    }
}

void VideoCanvas::mouseMoveEvent(QMouseEvent* event) {
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
    if (event->button() == Qt::LeftButton) {
        dragging_vertex_idx_ = -1;
        is_dragging_whole_ = false;
        if (mode_ == EditMode::EDITING) {
            editing_roi_.is_valid = validate_polygon(editing_roi_.vertices);
        }
    }
}

bool VideoCanvas::validate_polygon(const std::vector<cv::Point2f>& pts) const {
    if (pts.size() < 3) return false;
    double area = cv::contourArea(pts);
    return area > 400.0;
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

void VideoCanvas::start_drawing() {
    editing_roi_.vertices.clear();
    editing_roi_.is_valid = false;
    mode_ = EditMode::DRAWING;
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
```

---

## 6. ONNX Runtime YOLOv8 推理封装 (`src/ai/OnnxDetector.cpp`)

```cpp
#include "OnnxDetector.hpp"
#include <onnxruntime_cxx_api.h>
#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>

class OnnxDetector::Impl {
public:
    Ort::Env env_{ORT_LOGGING_LEVEL_WARNING, "YOLOv8"};
    Ort::SessionOptions session_options_;
    std::unique_ptr<Ort::Session> session_;
    Ort::MemoryInfo memory_info_ = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    int input_w_ = 640;
    int input_h_ = 640;
    float conf_thresh_ = 0.45f;
    float nms_thresh_ = 0.50f;

    std::vector<const char*> input_names_ = {"images"};
    std::vector<const char*> output_names_ = {"output0"};

    bool init(const std::string& model_path, float conf_thresh, float nms_thresh) {
        conf_thresh_ = conf_thresh;
        nms_thresh_ = nms_thresh;

        session_options_.SetIntraOpNumThreads(2);
        session_options_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        try {
            OrtCUDAProviderOptions cuda_options{};
            cuda_options.device_id = 0;
            session_options_.AppendExecutionProvider_CUDA(cuda_options);
            std::cout << "[OnnxDetector] CUDA Execution Provider initialized." << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[OnnxDetector] CUDA init failed, falling back to CPU: " << e.what() << std::endl;
        }

        std::wstring wide_path(model_path.begin(), model_path.end());
        session_ = std::make_unique<Ort::Session>(env_, wide_path.c_str(), session_options_);
        return true;
    }

    bool detect(const cv::Mat& bgr_frame, std::vector<Detection>& detections) {
        detections.clear();
        if (bgr_frame.empty()) return false;

        float r = std::min((float)input_w_ / bgr_frame.cols, (float)input_h_ / bgr_frame.rows);
        int unpad_w = std::round(bgr_frame.cols * r);
        int unpad_h = std::round(bgr_frame.rows * r);
        int pad_w = (input_w_ - unpad_w) / 2;
        int pad_h = (input_h_ - unpad_h) / 2;

        cv::Mat resized;
        cv::resize(bgr_frame, resized, cv::Size(unpad_w, unpad_h));
        cv::Mat input_img(input_h_, input_w_, CV_8UC3, cv::Scalar(114, 114, 114));
        resized.copyTo(input_img(cv::Rect(pad_w, pad_h, unpad_w, unpad_h)));

        cv::Mat rgb;
        cv::cvtColor(input_img, rgb, cv::COLOR_BGR2RGB);
        rgb.convertTo(rgb, CV_32FC3, 1.0f / 255.0f);

        std::vector<float> input_tensor_values(1 * 3 * input_h_ * input_w_);
        int channel_size = input_h_ * input_w_;
        std::vector<cv::Mat> channels(3);
        for (int i = 0; i < 3; ++i) {
            channels[i] = cv::Mat(input_h_, input_w_, CV_32FC1, input_tensor_values.data() + i * channel_size);
        }
        cv::split(rgb, channels);

        std::array<int64_t, 4> input_shape = {1, 3, input_h_, input_w_};
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info_, input_tensor_values.data(), input_tensor_values.size(),
            input_shape.data(), input_shape.size()
        );

        auto output_tensors = session_->Run(
            Ort::RunOptions{nullptr},
            input_names_.data(), &input_tensor, 1,
            output_names_.data(), 1
        );

        float* raw_output = output_tensors[0].GetTensorMutableData<float>();
        auto output_shape = output_tensors[0].GetTensorTypeAndShapeInfo().GetShape();
        int dimensions = output_shape[1]; // 84
        int num_proposals = output_shape[2]; // 8400

        std::vector<cv::Rect> boxes;
        std::vector<float> confidences;

        for (int i = 0; i < num_proposals; ++i) {
            float person_score = raw_output[4 * num_proposals + i];
            if (person_score >= conf_thresh_) {
                float cx = raw_output[0 * num_proposals + i];
                float cy = raw_output[1 * num_proposals + i];
                float w = raw_output[2 * num_proposals + i];
                float h = raw_output[3 * num_proposals + i];

                float orig_x = (cx - pad_w) / r - (w / (2.0f * r));
                float orig_y = (cy - pad_h) / r - (h / (2.0f * r));
                float orig_w = w / r;
                float orig_h = h / r;

                boxes.emplace_back(cv::Rect(std::round(orig_x), std::round(orig_y), std::round(orig_w), std::round(orig_h)));
                confidences.push_back(person_score);
            }
        }

        std::vector<int> indices;
        cv::dnn::NMSBoxes(boxes, confidences, conf_thresh_, nms_thresh_, indices);

        for (int idx : indices) {
            Detection det;
            det.bbox = cv::Rect2f(boxes[idx]);
            det.confidence = confidences[idx];
            det.class_id = 0;
            detections.push_back(det);
        }

        return true;
    }
};
```

---

## 7. 区域滞后防抖状态机 (`src/analytics/RoiCounter.cpp`)

```cpp
#include "RoiCounter.hpp"
#include <opencv2/imgproc.hpp>
#include <unordered_map>

enum class TrackState { UNKNOWN, OUTSIDE, INSIDE };

struct TrackContext {
    TrackState state = TrackState::UNKNOWN;
    int inside_counter = 0;
    int outside_counter = 0;
    int cooldown_ticks = 0;
};

class RoiCounter::Impl {
public:
    std::shared_ptr<const RoiConfig> roi_config_;
    std::unordered_map<int, TrackContext> track_states_;

    void process_frame(const std::vector<TrackObject>& tracks,
                       int64_t timestamp_ms,
                       int& current_occupancy,
                       std::vector<PassageEvent>& out_events) {
        current_occupancy = 0;
        out_events.clear();

        if (!roi_config_ || !roi_config_->is_valid || roi_config_->vertices.size() < 3) {
            return;
        }

        const auto& poly = roi_config_->vertices;
        float buf_dist = roi_config_->buffer_distance;
        int min_confirm = roi_config_->min_confirm_frames;

        for (const auto& trk : tracks) {
            double dist = cv::pointPolygonTest(poly, trk.foot_point, true);
            if (dist >= 0) {
                current_occupancy++;
            }

            auto& ctx = track_states_[trk.track_id];
            if (ctx.cooldown_ticks > 0) ctx.cooldown_ticks--;

            if (ctx.state == TrackState::UNKNOWN) {
                if (dist >= buf_dist) ctx.state = TrackState::INSIDE;
                else if (dist <= -buf_dist) ctx.state = TrackState::OUTSIDE;
                continue;
            }

            if (ctx.state == TrackState::OUTSIDE) {
                if (dist >= buf_dist) {
                    ctx.inside_counter++;
                    if (ctx.inside_counter >= min_confirm && ctx.cooldown_ticks == 0) {
                        ctx.state = TrackState::INSIDE;
                        ctx.inside_counter = 0;
                        ctx.cooldown_ticks = 30;

                        PassageEvent ev;
                        ev.timestamp_ms = timestamp_ms;
                        ev.track_id = trk.track_id;
                        ev.direction = PassageDirection::ENTRY;
                        ev.roi_version = roi_config_->version;
                        ev.confidence = trk.score;
                        ev.source_id = roi_config_->source_id;
                        ev.gender = trk.gender;
                        ev.age_group = trk.age_group;
                        out_events.push_back(ev);
                    }
                } else {
                    ctx.inside_counter = 0;
                }
            } else if (ctx.state == TrackState::INSIDE) {
                if (dist <= -buf_dist) {
                    ctx.outside_counter++;
                    if (ctx.outside_counter >= min_confirm && ctx.cooldown_ticks == 0) {
                        ctx.state = TrackState::OUTSIDE;
                        ctx.outside_counter = 0;
                        ctx.cooldown_ticks = 30;

                        PassageEvent ev;
                        ev.timestamp_ms = timestamp_ms;
                        ev.track_id = trk.track_id;
                        ev.direction = PassageDirection::EXIT;
                        ev.roi_version = roi_config_->version;
                        ev.confidence = trk.score;
                        ev.source_id = roi_config_->source_id;
                        ev.gender = trk.gender;
                        ev.age_group = trk.age_group;
                        out_events.push_back(ev);
                    }
                } else {
                    ctx.outside_counter = 0;
                }
            }
        }
    }

    void reset() { track_states_.clear(); }
};

RoiCounter::RoiCounter() : impl_(std::make_unique<Impl>()) {}
RoiCounter::~RoiCounter() = default;

void RoiCounter::set_roi_config(std::shared_ptr<const RoiConfig> config) {
    impl_->roi_config_ = config;
    impl_->reset();
}

void RoiCounter::process_frame(const std::vector<TrackObject>& tracks,
                              int64_t timestamp_ms,
                              int& current_occupancy,
                              std::vector<PassageEvent>& out_events) {
    impl_->process_frame(tracks, timestamp_ms, current_occupancy, out_events);
}

void RoiCounter::reset() { impl_->reset(); }
```

---

## 8. 短视频切片与 SQLite WAL 存盘

```cpp
// src/video/ClipRecorder.hpp
#pragma once
#include <opencv2/opencv.hpp>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

class ClipRecorder {
public:
    explicit ClipRecorder(size_t max_buffer_frames = 90) : max_buffer_size_(max_buffer_frames) {}

    void push_frame(const cv::Mat& frame) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (frame_buffer_.size() >= max_buffer_size_) {
            frame_buffer_.pop_front();
        }
        frame_buffer_.push_back(frame.clone());
    }

    std::string save_clip_async(const std::string& output_dir, int track_id, int64_t timestamp_ms, int fps = 25) {
        std::vector<cv::Mat> snapshot;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            snapshot.assign(frame_buffer_.begin(), frame_buffer_.end());
        }

        if (snapshot.empty()) return "";

        std::string filename = output_dir + "/clip_" + std::to_string(timestamp_ms) + 
                               "_t" + std::to_string(track_id) + ".mp4";

        std::thread([snapshot = std::move(snapshot), filename, fps]() {
            if (snapshot.empty()) return;
            int w = snapshot[0].cols;
            int h = snapshot[0].rows;
            cv::VideoWriter writer(filename, cv::VideoWriter::fourcc('a', 'v', 'c', '1'), fps, cv::Size(w, h));
            if (!writer.isOpened()) {
                writer.open(filename, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), fps, cv::Size(w, h));
            }
            for (const auto& f : snapshot) {
                writer.write(f);
            }
            writer.release();
        }).detach();

        return filename;
    }

private:
    size_t max_buffer_size_;
    std::deque<cv::Mat> frame_buffer_;
    std::mutex mtx_;
};
```

```cpp
// src/storage/DatabaseManager.cpp
#include "DatabaseManager.hpp"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

bool DatabaseManager::init_database(const QString& db_path) {
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(db_path);

    if (!db.open()) {
        qCritical() << "Failed to open database:" << db.lastError().text();
        return false;
    }

    QSqlQuery q;
    q.exec("PRAGMA journal_mode = WAL;");
    q.exec("PRAGMA synchronous = NORMAL;");

    bool ok = q.exec(
        "CREATE TABLE IF NOT EXISTS passage_events ("
        "  event_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  source_id TEXT NOT NULL,"
        "  timestamp_ms INTEGER NOT NULL,"
        "  track_id INTEGER NOT NULL,"
        "  direction INTEGER NOT NULL,"
        "  roi_version INTEGER NOT NULL,"
        "  confidence REAL NOT NULL,"
        "  clip_path TEXT DEFAULT '',"
        "  gender TEXT DEFAULT '未知',"
        "  age_group TEXT DEFAULT '未知'"
        ");"
    );
    q.exec("CREATE INDEX IF NOT EXISTS idx_events_ts ON passage_events(timestamp_ms);");
    return ok;
}

bool DatabaseManager::insert_events(const std::vector<PassageEvent>& events) {
    if (events.empty()) return true;

    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    QSqlQuery q;
    q.prepare("INSERT INTO passage_events ("
              "source_id, timestamp_ms, track_id, direction, roi_version, confidence, clip_path, gender, age_group) "
              "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");

    for (const auto& ev : events) {
        q.addBindValue(QString::fromStdString(ev.source_id));
        q.addBindValue(static_cast<qint64>(ev.timestamp_ms));
        q.addBindValue(ev.track_id);
        q.addBindValue(static_cast<int>(ev.direction));
        q.addBindValue(ev.roi_version);
        q.addBindValue(ev.confidence);
        q.addBindValue(QString::fromStdString(ev.clip_path));
        q.addBindValue(QString::fromStdString(ev.gender));
        q.addBindValue(QString::fromStdString(ev.age_group));
        q.exec();
    }

    return db.commit();
}
```
