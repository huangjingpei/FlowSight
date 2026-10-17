# FlowSight 客流统计系统 — MVP 技术原型与规格说明书

- **文档版本**：v1.2（多页面商业架构演进版）
- **状态**：MVP 架构与系统规格说明 / 落地指导
- **目标平台**：Windows 10/11 + NVIDIA GPU（第一阶段落地）；Linux + RK3588 NPU（嵌入式轻量兼顾）
- **核心技术栈**：C++17 / Qt 6.5+ / OpenCV 4.8+ / ONNX Runtime 1.16+ (CUDA 11.8/12.x) / ByteTrack / SQLite3
- **系统核心形态**：**侧边栏导航 + 多页面工作区路由模式（Multi-Page View Architecture）**，彻底告别单页面拥挤与误触，包含【实时监控中心】、【空间标定工作台】、【事件与视频回溯中心】、【数据统计与报表看板】及【系统与存储设置】五大独立模块。

---

## 1. 项目概述

### 1.1 项目背景与工业级设计痛点

在商业商铺走廊、写字楼出入口、地铁站通道等实际安防与客流场景中，传统单页面（Single Page）工具软件存在致命弊端：
1. **误触风险极高**：日常安保或店长只需看实时客流大字和监控画面，若将“ROI 绘制”、“删除”、“参数阈值滑动条”堆在主屏，极易发生误触导致已标定几何区域损毁；
2. **视觉利用率低**：单页面三栏挤压导致视频画幅被压至 50% 以下，微小目标难以分辨；报表趋势图挤成一条缝，缺乏历史洞察力；
3. **证据链断层**：客流系统不能仅仅呈现冰冷的数字累加，客户与运营人员需要“每一笔数据均有据可查”的视频证据回溯能力。

FlowSight 采用成熟工业安防平台（如海康 iVMS、大华 DSS、百度度目）的标准设计范式，通过 **`QStackedWidget` 实现五大独立工作区路由**，实现日常监控、工程标定、证据追溯与报表决策的严格业务解耦。

### 1.2 MVP 目标边界

1. **输入与循环分析**：直接使用单路本地高清视频文件（MP4/H.264，支持循环播放）进行开发调试与功能闭环，兼顾 RTSP 网络流。
2. **高精度检测与跟踪**：集成 YOLOv8n (ONNX) 行人检测器与 ByteTrack 多目标跟踪器，输出平滑轨迹。
3. **多页面工作区路由**：
   - **页面 1：【实时监控中心】(Live Monitor)**：沉浸式大屏视频呈现，只读渲染 ROI 与目标框，右侧极简卡片展示当前人数、今日进出累计，底部呈现最新通行走字动态卡片。
   - **页面 2：【空间标定工作台】(Calibration Studio)**：专用于工程标定。支持冻结抓帧/实时叠加，支持任意 $N \ge 3$ 边形自由单击打点、双击闭合、顶点微调、多边形平移，参数滑动条调节与滞后缓冲带可视化。
   - **页面 3：【事件与视频回溯中心】(Evidence Archive)**：事件审计流水列表，按时间/方向/属性筛选，支持双击记录在内置播放器中秒级回放 2~3 秒 MP4 验证短视频。
   - **页面 4：【数据统计与报表看板】(Analytics & Reports)**：24小时进出分布堆叠柱状图、客流峰谷折线图、性别/年龄段属性画像环形图，支持导出 Excel/CSV。
   - **页面 5：【系统与存储设置】(Settings)**：视频源切换、存储目录管理、磁盘切片自动清理策略（如保留 30 天）。
4. **空间过滤与双阈值滞后防抖**：采用底边中心自适应代表点，结合 OpenCV `pointPolygonTest` 带符号距离划分内部核心区、外部核心区与过渡缓冲带，消灭边界徘徊误报。
5. **事件视频切片存盘（Clip Archiving）**：后台维护环形帧缓冲（RingBuffer），进出触发时异步将前后 60~90 帧写为小 MP4 文件，路径写入 SQLite。
6. **架构解耦与嵌入式兼容**：抽象 `IDetector` 纯虚接口；代码全量基于标准 C++17、Qt6、OpenCV 与 CMake，杜绝 Windows 私有 API，为后续平移 RK3588 扫清阻碍。

---

## 2. 系统多页面架构与路由模型

```text
+----------------------------------------------------------------------------------------------------+
| QMainWindow                                                                          [-] [口] [X]   |
+--------+-------------------------------------------------------------------------------------------+
| (导航) |  QStackedWidget (工作区视图路由容器)                                                       |
|        |                                                                                           |
| [ 监控 ]  ===> MonitorWidget   : 沉浸式全屏预览 + 核心 KPI 大字卡片 + 最新动态滚动 (防误触只读模式) |
|        |                                                                                           |
| [ 标定 ]  ===> StudioWidget    : 标定大画布 + N 边形绘制/拉伸 + 缓冲带可视化 + 参数实时调节与生效  |
|        |                                                                                           |
| [ 证据 ]  ===> EvidenceWidget  : 进出审计流水表 + 日期/方向筛选 + 3 秒验证短视频内置秒级回放       |
|        |                                                                                           |
| [ 报表 ]  ===> AnalyticsWidget : 24小时柱状图 + 进出对比折线图 + 属性画像环形图 + 导出 Excel/CSV  |
|        |                                                                                           |
| [ 设置 ]  ===> SettingsWidget  : 视频源配置 + 存储配额与自动清理 + AI 硬件信息展示                 |
+--------+-------------------------------------------------------------------------------------------+
| 底部全局状态栏: [设备: 正常] | [推理: CUDA 14ms / 30 FPS] | [存储: WAL 模式正常] | [可用空间: 128G]        |
+----------------------------------------------------------------------------------------------------+
```

### 2.1 页面切换与状态同步机制

1. **共享流分发（Publisher-Subscriber）**：
   - 底层 `PipelineController` 工作线程持续采集与推理，生成 `QImage`、`std::vector<TrackObject>` 与统计数据，通过 Qt 信号统一向 UI 分发；
   - 当前位于【监控中心】时，`MonitorWidget` 订阅渲染，ROI 处于不可交互状态；
   - 切换至【标定工作台】时，`StudioWidget` 激活鼠标跟踪并接收交互，支持冻结画面以精细化打点；
2. **原子配置热生效**：
   - 在【标定工作台】点击“应用生效”后，构造 `std::shared_ptr<const RoiConfig>` 原子推送到 `RoiCounter`，并通知【监控中心】同步更新其只读多边形轮廓。

---

## 3. 核心算法、坐标与状态机规格

### 3.1 Letterbox 显示与双向坐标映射数学模型

Qt 的视频显示控件 `VideoCanvas` 大小为 $(W_c, H_c)$，输入视频原始分辨率为 $(W_f, H_f)$。在保持长宽比（`Qt::KeepAspectRatio`）绘制视频时，产生留白偏移量：

- 缩放尺度：$s = \min\left( \frac{W_c}{W_f}, \; \frac{H_c}{H_f} \right)$
- 渲染区域尺寸：$W_r = W_f \cdot s, \quad H_r = H_f \cdot s$
- 留白偏移：$dx = (W_c - W_r)/2, \quad dy = (H_c - H_r)/2$

**双向转换公式**：
1. **正向转换（原始帧像素 $\to$ 控件绘制像素）**：
   $$X_{\text{canvas}} = X_{\text{frame}} \cdot s + dx, \quad Y_{\text{canvas}} = Y_{\text{frame}} \cdot s + dy$$
2. **反向转换（鼠标点击 $\to$ 原始帧像素）**：
   $$X_{\text{frame}} = \text{std::clamp}\left( \frac{X_{\text{canvas}} - dx}{s}, \; 0.0f, \; W_f - 1.0f \right)$$
   $$Y_{\text{frame}} = \text{std::clamp}\left( \frac{Y_{\text{canvas}} - dy}{s}, \; 0.0f, \; H_f - 1.0f \right)$$

### 3.2 任意 N 边形几何测试与算力分析
- 允许用户定义 $N \ge 3$ 的闭合多边形，顶点按顺/逆时针排序保存在原始视频帧像素坐标中；
- 采用射线交点法 / OpenCV `cv::pointPolygonTest(pts, pt, true)`，时间复杂度为 $O(N)$；
- 算力实测：对于 $N=6$ 的多边形，单目标判定耗时约 $0.0004\text{ ms}$。整帧 20 个人判定耗时 $<0.01\text{ ms}$，相比 YOLO 推理（$14\text{ ms}$）开销完全可忽略。

### 3.3 自适应足底点计算
$$P_{\text{foot}} = \begin{cases} 
\left( \frac{x_1 + x_2}{2}, \; y_2 \right), & \text{若 } y_2 < H_{\text{frame}} - 3 \\ 
\left( \frac{x_1 + x_2}{2}, \; y_1 + 0.85(y_2 - y_1) \right), & \text{若 } y_2 \ge H_{\text{frame}} - 3 \text{ (底部裁切自适应)} 
\end{cases}$$

### 3.4 双阈值滞后几何缓冲带与状态机
定义带符号距离 $d = \text{cv::pointPolygonTest}(ROI, P_{\text{foot}}, \text{true})$ 与缓冲阈值 $\delta_{\text{buf}} = 20\text{ px}$：
- **核心内部（Core Inside）**：$d \ge +\delta_{\text{buf}}$
- **核心外部（Core Outside）**：$d \le -\delta_{\text{buf}}$
- **过渡缓冲带（Hysteresis Zone）**：$-\delta_{\text{buf}} < d < +\delta_{\text{buf}}$（状态锁定，严格不发生跃迁）

**状态转移规则**：
- `OUTSIDE` $\to$ 进入核心内部（$d \ge 20$）持续保持 $\ge 4$ 帧，触发 `ENTRY` 事件，开启 30 帧冷却，状态转为 `INSIDE`；
- `INSIDE` $\to$ 穿出至核心外部（$d \le -20$）持续保持 $\ge 4$ 帧，触发 `EXIT` 事件，开启 30 帧冷却，状态转为 `OUTSIDE`；
- 在缓冲带内来回晃动、踏步、转身，状态绝对不发生跳变，零误报。

---

## 4. 数据存储与短视频切片设计

### 4.1 核心 SQLite DDL (含切片与属性)

```sql
PRAGMA journal_mode = WAL;
PRAGMA synchronous = NORMAL;
PRAGMA foreign_keys = ON;

-- 1. 视频源表
CREATE TABLE IF NOT EXISTS video_sources (
    source_id VARCHAR(64) PRIMARY KEY,
    source_name VARCHAR(128) NOT NULL,
    source_type VARCHAR(16) NOT NULL,
    uri TEXT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 2. ROI 配置表 (支持 N 边形)
CREATE TABLE IF NOT EXISTS roi_configs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    source_id VARCHAR(64) NOT NULL,
    roi_version INTEGER NOT NULL,
    ref_width INTEGER NOT NULL,
    ref_height INTEGER NOT NULL,
    vertices_json TEXT NOT NULL,       -- JSON: [{"x":200,"y":300},{"x":400,"y":300}, ...]
    buffer_distance REAL DEFAULT 20.0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (source_id) REFERENCES video_sources(source_id)
);
CREATE UNIQUE INDEX IF NOT EXISTS idx_roi_source_ver ON roi_configs(source_id, roi_version);

-- 3. 通行事实明细表 (含短视频切片路径与属性)
CREATE TABLE IF NOT EXISTS passage_events (
    event_id INTEGER PRIMARY KEY AUTOINCREMENT,
    source_id VARCHAR(64) NOT NULL,
    timestamp_ms INTEGER NOT NULL,
    track_id INTEGER NOT NULL,
    direction INTEGER NOT NULL,        -- 1: 进入, 2: 离开
    roi_version INTEGER NOT NULL,
    confidence REAL NOT NULL,
    clip_path TEXT DEFAULT '',         -- 关联的 3 秒短视频文件路径 (如 "clips/clip_12.mp4")
    gender TEXT DEFAULT '未知',
    age_group TEXT DEFAULT '未知',
    FOREIGN KEY (source_id) REFERENCES video_sources(source_id)
);
CREATE INDEX IF NOT EXISTS idx_events_ts ON passage_events(timestamp_ms);

-- 4. 小时聚合统计报表
CREATE TABLE IF NOT EXISTS stats_hourly (
    source_id VARCHAR(64) NOT NULL,
    stat_hour VARCHAR(13) NOT NULL,
    entry_count INTEGER DEFAULT 0,
    exit_count INTEGER DEFAULT 0,
    peak_occupancy INTEGER DEFAULT 0,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (source_id, stat_hour)
);
```

### 4.2 目标短视频切片管理与回溯

1. **内存环形缓冲区**：采集线程每帧往 `std::deque<cv::Mat>` 写入帧，维持约 90 帧（3秒）；
2. **事件触发切片**：`RoiCounter` 判定 `ENTRY` 或 `EXIT` 时，异步派发落盘任务，用 OpenCV `VideoWriter` 将这 90 帧写出为轻量 MP4 文件；
3. **UI 证据回溯**：在【事件与视频回溯中心】页面，双击记录即可调起内置播放器进行现场复核。

---

## 5. 验收标准与测试用例矩阵

| 编号 | 测试场景 | 操作/输入 | 预期结果 |
|---|---|---|---|
| **TC-NAV-01** | 多页面路由切换 | 点击左侧 5 个导航图标 | 页面平滑瞬切，后台视频与推理不中断、不丢帧 |
| **TC-ROI-01** | N 边形绘制与闭合 | 单击 5 个顶点后双击闭合 | 生成 5 边形黄色半透明蒙版，控制点可拖拽 |
| **TC-ROI-02** | 监控页防误触 | 在【实时监控】页面点击拖拽视频 | 画面无任何位移或改动，配置严格受保护 |
| **TC-ALG-01** | 单向穿越进出 | 行人完全穿入/离开 ROI | 监控看板进入/离开 +1，状态机平稳切换 |
| **TC-ALG-02** | 边界徘徊抗抖 | 行人在边界线上反复踏步徘徊 5 秒 | 缓冲带锁定状态，进出计数保持不变，零误报 |
| **TC-EVI-01** | 证据短视频切片 | 发生进出后进入【证据回溯】双击行 | 毫秒级起播对应 3 秒短视频，画面完整清晰 |
| **TC-DATA-01**| 报表导出 Excel | 点击【数据报表】导出 CSV | 正确生成当天下发小时级客流与属性汇总数据 |

---

## 6. 模型解耦与嵌入式轻量兼顾

1. **接口解耦**：抽象 `IDetector` 纯虚类，输出统一的 `std::vector<Detection>`；
2. **更换模型无缝切换**：MVP 使用 `yolov8n.onnx`。若未来需商用规避 GPL-3.0 协议，替换为 MIT/Apache 协议模型（如 YOLOv6、YOLOv10、PP-YOLOE）时，**仅需修改几十行张量解析代码，上层 UI、跟踪和多页面架构完全无需改动**；
3. **跨平台兼顾**：全套代码使用标准 C++17、Qt6、OpenCV 与 CMake，禁止使用 Windows 私有 API，后续平移至 RK3588 Ubuntu 平台仅需编写对应的 `RknnDetector.cpp`。
