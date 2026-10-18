# FlowSight 智能客流分析系统 (FlowSight AI Analytics)

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-17-blue.svg" alt="C++17">
  <img src="https://img.shields.io/badge/Qt-6.9.2-green.svg" alt="Qt 6">
  <img src="https://img.shields.io/badge/OpenCV-4.8.0-red.svg" alt="OpenCV">
  <img src="https://img.shields.io/badge/ONNX_Runtime-1.24-brightgreen.svg" alt="ONNX Runtime">
  <img src="https://img.shields.io/badge/Acceleration-NVIDIA_CUDA-76B900.svg" alt="CUDA">
  <img src="https://img.shields.io/badge/Tracking-ByteTrack-orange.svg" alt="ByteTrack">
  <img src="https://img.shields.io/badge/Database-SQLite_WAL-blueviolet.svg" alt="SQLite WAL">
</p>

**FlowSight** 是一款面向商业商铺走廊、写字楼通道、地铁出入口等现实场景的**工业级智能客流分析与行为感知系统**。系统基于现代 C++17、Qt 6、OpenCV 以及 ONNX Runtime 构建，彻底告别传统客流软件生硬的单页面挤压设计，采用成熟安防平台标准的**五大独立工作区路由架构**。

---

## 🌟 核心特性与架构亮点

### 1. 商业级多页面工作区架构 (Multi-Page View Architecture)
采用 **Qt 侧边导航栏 + `QStackedWidget` 页面路由模式**，将监控、标定与核算职责彻底解耦：
- **📹 实时监控中心 (Live Monitor)**：沉浸式 85% 大屏视频呈现，只读防误触设计，大字卡片呈现当前活跃人数、今日进出累计；
- **📐 空间标定工作台 (Calibration Studio)**：专用于空间几何标定，支持抓帧冻结与实时叠加，支持**任意 $N \ge 3$ 边形自由单击打点、双击闭合、顶点控制球微调与整体平移**，滞后缓冲带可视化；
- **🎬 事件与视频回溯中心 (Evidence Archive)**：通行事实明细流水，集成微型播放器，**双击任意记录秒级弹窗回放该行人进出的 2~3 秒真实短视频切片**，每一笔数据均有据可查；
- **📊 数据统计与报表看板 (Data Analytics)**：24小时进出分布堆叠直方图、客群性别/年龄画像概览，支持一键导出 Excel / CSV 报表；
- **⚙️ 系统与存储设置 (Settings)**：视频输入源切换（本地 MP4 / RTSP 流）与短视频切片磁盘自动清理策略。

### 2. 精准几何与双阈值滞后防抖状态机 (Hysteresis Buffer Zone)
- **自适应足底代表点**：常规目标取底边中心点，当目标贴近画面底边缘被裁切时自动启用下四分位回退点；
- **双阈值几何缓冲带**：利用 OpenCV `pointPolygonTest` 距离场严格划分核心内部、核心外部与过渡缓冲带。行人在边界踏步、转身徘徊时状态严格锁定，**实现零踏步误报**。

### 3. 高精度检测与多目标跟踪 (YOLOv8 + ByteTrack)
- **模型解耦**：统一抽象 `IDetector` 纯虚接口，输入输出解耦，未来更换为 MIT/Apache 协议商业模型仅需改动数十行代码；
- **ByteTrack 跟踪**：基于高低分双阶段关联与卡尔曼滤波，有效抗交叉遮挡。

### 4. 现代化暗黑科技美学 (Dark Slate Cyber Theme)
- 采用深黑曜石色调与高科技电光青、翡翠绿微光配色；
- 目标跟踪框升级为**四角 HUD 科技拐角支架（Corner Brackets）**与发光双层同心足底点；
- 全局无衬线现代排版与极细扁平圆角滚动条。

---

## 📂 工程目录结构

```text
FlowSight/
├── CMakeLists.txt                 # 顶层 CMake 构建脚本
├── docs/                          # 规格与实施手册
│   ├── 00-FlowSight_MVP_技术原型.md              # 架构规格说明书
│   ├── 01-FlowSight_MVP_落地实施指南与代码骨架.md   # C++ / Qt 核心代码实现手册
│   └── 02-FlowSight_MVP_全景操作与部署手册.md       # 全景操作与演示验收 SOP
├── src/
│   ├── common/                    # 公共类型与线程安全队列
│   ├── ai/                        # 检测器模块 (IDetector 抽象与 OnnxDetector)
│   ├── tracking/                  # 跟踪适配器 (ByteTrackWrapper)
│   ├── analytics/                 # 区域判定与滞后状态机 (RoiCounter)
│   ├── storage/                   # SQLite WAL 并发存储 (DatabaseManager)
│   ├── video/                     # 采集与短视频切片录制 (ClipRecorder)
│   ├── pipeline/                  # 流水线控制器 (PipelineController)
│   └── ui/                        # Qt 现代化多页面界面
│       ├── Theme.hpp              # 全局暗黑科技 QSS 主题
│       ├── VideoCanvas.hpp/.cpp   # HUD 视频画布与 N 边形标定交互
│       ├── views/                 # 五大核心独立工作区视图
│       ├── MainWindow.hpp/.cpp    # 侧边导航与路由总控
│       └── main.cpp
└── clips/                         # 通行事件自动短视频切片存储目录
```

---

## 🛠️ 构建与运行指南

### 环境要求
- **操作系统**：Windows 10 / 11 64-bit
- **编译器**：MSVC v143 (Visual Studio 2022)
- **构建工具**：CMake 3.24+
- **GUI 框架**：Qt 6.5+ (Widgets, Sql, Multimedia)
- **视觉库**：OpenCV 4.8+
- **推理运行时**：ONNX Runtime 1.16+ (CUDA / CPU)

### 一键构建命令 (Windows MSVC)
```powershell
# 1. 配置 CMake (自动识别本机 Qt 与 OpenCV)
cmake -B build -G "Visual Studio 17 2022" -A x64

# 2. 编译 Release 版本
cmake --build build --config Release -j

# 3. 部署 Qt 运行时依赖
& "C:\Qt\6.9.2\msvc2022_64\bin\windeployqt.exe" --release build\Release\FlowSight.exe

# 4. 运行系统
.\build\Release\FlowSight.exe
```

> **提示**：系统内置了**动态走廊与活动行人模拟生成器**。即使用户尚未放入真实的测试视频文件，启动后也能立即进行目标检测、轨迹跟踪与进出状态机全链路演示！

---

## 📖 技术文档指南

- [**00-FlowSight MVP 技术原型与规格说明书**](docs/00-FlowSight_MVP_技术原型.md)：业务定义、统计口径、数学逆映射模型与 DDL。
- [**01-FlowSight MVP 落地实施指南与代码骨架**](docs/01-FlowSight_MVP_落地实施指南与代码骨架.md)：核心类设计、现代 CMake 配置与 7 天实施路线图。
- [**02-FlowSight MVP 全景操作与部署手册 (SOP)**](docs/02-FlowSight_MVP_全景操作与部署手册.md)：分页面操作 SOP、机位调优指南与 5 分钟客户演示剧本。
- [**03-FlowSight 系统核心技术架构与跨平台移植规范**](docs/03-FlowSight_系统核心技术架构_时序流程与跨平台移植规范.md)：线程拓扑时序、零拷贝渲染与 C-ABI 导出规划。
- [**04-FlowSight 核心疑虑与技术决议剖析**](docs/04-FlowSight_核心疑虑与技术决议剖析.md)：级联属性分类、卡尔曼调优与多路流矩阵论证。
- [**05-FlowSight 硬件推理智能探测与 CPU 平滑 Fallback**](docs/05-FlowSight_硬件推理环境智能探测与CPU平滑Fallback容灾方案.md)：CUDA/CPU 双模智能自适应与容灾策略。
- [**06-FlowSight 商业化核心功能缺口分析与分期演进路线图**](docs/06-FlowSight_商业化核心功能缺口分析与分期演进路线图.md)：商业化 6 大功能缺口深度审计、P0/P1/P2 分期实施与状态跟踪矩阵。

---

## 📄 开源与商业许可
本项目基于 MIT 协议分发，算法与模型接口已实现全面解耦。
