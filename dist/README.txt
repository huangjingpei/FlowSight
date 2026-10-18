================================================================================
  FlowSight - 商业级工业视觉客流统计与智能分析系统 (v1.2.0 商业便携版)
================================================================================

感谢您使用 FlowSight 工业视觉商业客流分析套件！
本版本为纯净绿色免安装版，内置全套运行时依赖（Qt6、OpenCV、ONNX Runtime）
与预置深度学习感知模型（YOLOv8n + MobileNetV3），开箱即用。

【系统要求】
- 操作系统: 64 位 Windows 10 / Windows 11 / Windows Server 2019+
- 硬件配置: 
  * CPU: 4 核心及以上 x86_64 处理器 (支持 AVX 指令集)
  * 内存: 4GB RAM 及以上 (推荐 8GB)
  * 显卡: 可选，支持 NVIDIA CUDA 加速 (无显卡时自动启用 CPU 多线程高性能模式)
  * 磁盘: 至少 500MB 可用空间

【快速启动】
1. 双击运行当前目录下的 "FlowSight.exe"；
2. 系统启动后将自动探查本地过道演示视频 ("configs/test_corridor.mp4") 并开箱演示；
3. 如需接入网络监控摄像头：
   在界面点击切换视频源，输入标准 RTSP 地址，例如:
   rtsp://admin:password@192.168.1.64:554/h264/ch1/main/av_stream
4. 标定工作台支持任意多边形交互标定，支持反转通行判定朝向与多防区独立核算。

【开发者 SDK 说明】
- "sdk/include/FlowSightAPI.h": 纯标准 C-ABI 头文件，零 Qt 依赖；
- "sdk/lib/FlowSightEngine.lib" 与 "FlowSightEngine.dll":
  支持在 C#、Java、Python、C/C++ 等第三方上位机系统中以动态库形式直接嵌入
  FlowSight 核心算法引擎。

【目录说明】
- FlowSight.exe            : 商业客户端主程序
- FlowSightEngine.dll      : C-ABI 核心感知引擎动态链接库
- models/                  : 深度学习模型权重 (YOLOv8n + MobileNetV3)
- configs/                 : 配置文件与演示视频
- data/                    : 运行日志与 SQLite WAL 数据库
- clips/                   : 进出通行事件触发的现场短视频存盘目录
- exports/                 : 商业智能报表 CSV 导出目录
- sdk/                     : 面向第三方平台集成的 SDK 组件

【技术支持与商务合作】
GitHub: https://github.com/huangjingpei/FlowSight
版权所有 (C) 2026 FlowSight 商业化团队。保留所有权利。
================================================================================
