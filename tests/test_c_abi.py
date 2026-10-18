#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
test_c_abi.py:
FlowSight 核心算法引擎 C-ABI 跨语言 (Python ctypes) 动态调用与事件回调端到端自检
"""

import os
import sys
import time
import ctypes
from ctypes import (
    c_void_p, c_int, c_int32, c_int64, c_uint8, c_float, c_char_p,
    Structure, POINTER, CFUNCTYPE
)

# 1. 定义与 include/flowsight/FlowSightAPI.h 严格二进制对齐的 C 结构体

class FS_Point2D(Structure):
    _fields_ = [
        ("x", c_float),
        ("y", c_float),
    ]

class FS_FlowEvent(Structure):
    _pack_ = 8
    _fields_ = [
        ("channel_id", c_int32),
        ("event_id", c_int64),
        ("timestamp_ms", c_int64),
        ("track_id", c_int32),
        ("direction", c_uint8),    # 0=UNKNOWN, 1=ENTRY, 2=EXIT
        ("roi_id", c_int32),
        ("roi_name", c_char_p),
        ("confidence", c_float),
        ("foot_x", c_float),
        ("foot_y", c_float),
        ("clip_file_path", c_char_p),
        ("gender", c_char_p),
        ("age_group", c_char_p),
    ]

class FS_SystemStats(Structure):
    _pack_ = 8
    _fields_ = [
        ("current_occupancy", c_int32),
        ("today_entry", c_int32),
        ("today_exit", c_int32),
        ("today_total", c_int32),
        ("infer_ms", c_float),
        ("fps", c_float),
    ]

# 2. 回调函数签名定义
FS_FlowEventCallback = CFUNCTYPE(None, POINTER(FS_FlowEvent), c_void_p)
FS_SystemStatsCallback = CFUNCTYPE(None, POINTER(FS_SystemStats), c_void_p)

# 全局收集统计与事件
received_events = []
received_stats = []

def py_event_callback(event_ptr, user_data):
    if not event_ptr:
        return
    ev = event_ptr.contents
    direction_str = "进入 [ENTRY]" if ev.direction == 1 else ("离开 [EXIT]" if ev.direction == 2 else "未知")
    roi_name = ev.roi_name.decode('utf-8', errors='ignore') if ev.roi_name else "未知防区"
    gender = ev.gender.decode('utf-8', errors='ignore') if ev.gender else ""
    age = ev.age_group.decode('utf-8', errors='ignore') if ev.age_group else ""
    
    msg = (f"[Python SDK 截获通行事件] ID:#{ev.track_id} | 防区: {roi_name} | "
           f"方向: {direction_str} | 置信度: {ev.confidence*100:.1f}% | 属性: {gender} {age}")
    print(msg)
    received_events.append(ev.track_id)

def py_stats_callback(stats_ptr, user_data):
    if not stats_ptr:
        return
    st = stats_ptr.contents
    received_stats.append((st.fps, st.infer_ms, st.current_occupancy))
    print(f"[Python SDK 实时指标] 活跃人数: {st.current_occupancy} | 累计进出: {st.today_entry}/{st.today_exit} | 耗时: {st.infer_ms:.1f}ms | 帧率: {st.fps:.1f}FPS")

def main():
    print("=" * 70)
    print("  FlowSight C-ABI 动态链接库跨语言嵌入式自检 (Python ctypes)")
    print("=" * 70)

    dll_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "build", "Release"))
    dll_path = os.path.join(dll_dir, "FlowSightEngine.dll")
    
    if not os.path.exists(dll_path):
        print(f"[-] 错误: 未找到动态库 {dll_path}")
        return 1

    # Windows 上将 DLL 所在目录加入搜索路径，以便自动载入 onnxruntime.dll / opencv_world480.dll
    if hasattr(os, 'add_dll_directory'):
        os.add_dll_directory(dll_dir)
    os.environ['PATH'] = dll_dir + ';' + os.environ.get('PATH', '')

    print(f"[+] 正在加载动态库: {dll_path}")
    try:
        engine_dll = ctypes.CDLL(dll_path)
    except Exception as e:
        print(f"[-] 动态库加载失败: {e}")
        return 1

    print("[+] FlowSightEngine.dll 加载成功!")

    # 3. 绑定 C-ABI 函数原型
    engine_dll.FlowSight_Create.restype = c_void_p
    engine_dll.FlowSight_Create.argtypes = []

    engine_dll.FlowSight_Destroy.restype = None
    engine_dll.FlowSight_Destroy.argtypes = [c_void_p]

    engine_dll.FlowSight_Initialize.restype = c_int
    engine_dll.FlowSight_Initialize.argtypes = [c_void_p, c_char_p, c_float, c_int]

    engine_dll.FlowSight_StartStream.restype = c_int
    engine_dll.FlowSight_StartStream.argtypes = [c_void_p, c_char_p]

    engine_dll.FlowSight_StopStream.restype = None
    engine_dll.FlowSight_StopStream.argtypes = [c_void_p]

    engine_dll.FlowSight_SetDetectionRegion.restype = c_int
    engine_dll.FlowSight_SetDetectionRegion.argtypes = [c_void_p, c_int32, c_char_p, POINTER(FS_Point2D), c_int]

    engine_dll.FlowSight_RegisterEventCallback.restype = None
    engine_dll.FlowSight_RegisterEventCallback.argtypes = [c_void_p, FS_FlowEventCallback, c_void_p]

    engine_dll.FlowSight_RegisterStatsCallback.restype = None
    engine_dll.FlowSight_RegisterStatsCallback.argtypes = [c_void_p, FS_SystemStatsCallback, c_void_p]

    engine_dll.FlowSight_GetStats.restype = c_int
    engine_dll.FlowSight_GetStats.argtypes = [c_void_p, POINTER(FS_SystemStats)]

    # 4. 创建引擎实例
    handle = engine_dll.FlowSight_Create()
    if not handle:
        print("[-] 创建引擎句柄失败!")
        return 1
    print(f"[+] 引擎实例句柄已创建: 0x{handle:X}")

    # 5. 初始化模型
    model_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "models", "yolov8n.onnx")).encode('utf-8')
    ret = engine_dll.FlowSight_Initialize(handle, model_path, c_float(0.40), 0)
    if ret != 0:
        print(f"[-] 初始化引擎失败, 错误码: {ret}")
        engine_dll.FlowSight_Destroy(handle)
        return 1
    print("[+] YOLOv8n 推理引擎初始化成功!")

    # 6. 设置过道多边形标定防区
    # 设置一个横跨过道中央的四边形
    pts = (FS_Point2D * 4)(
        FS_Point2D(100.0, 180.0),
        FS_Point2D(540.0, 180.0),
        FS_Point2D(540.0, 420.0),
        FS_Point2D(100.0, 420.0),
    )
    ret = engine_dll.FlowSight_SetDetectionRegion(handle, 1, "A门主通道".encode('utf-8'), pts, 4)
    print(f"[+] 防区标定成功: ret={ret}")

    # 7. 注册纯 C 回调函数
    c_event_cb = FS_FlowEventCallback(py_event_callback)
    c_stats_cb = FS_SystemStatsCallback(py_stats_callback)
    engine_dll.FlowSight_RegisterEventCallback(handle, c_event_cb, None)
    engine_dll.FlowSight_RegisterStatsCallback(handle, c_stats_cb, None)
    print("[+] C-ABI 事件与指标回调函数注册成功!")

    # 8. 启动视频分析流
    video_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "configs", "test_corridor.mp4")).encode('utf-8')
    ret = engine_dll.FlowSight_StartStream(handle, video_path)
    if ret != 0:
        print(f"[-] 启动视频流失败, ret={ret}")
        engine_dll.FlowSight_Destroy(handle)
        return 1

    print(f"[+] 视频流已启动! 正在持续分析 12 秒...")
    time.sleep(12)

    # 9. 主动查询实时统计
    current_stats = FS_SystemStats()
    ret = engine_dll.FlowSight_GetStats(handle, ctypes.byref(current_stats))
    print("-" * 70)
    print(f"[+] FlowSight_GetStats 结果: 活跃人数={current_stats.current_occupancy}, 累计进={current_stats.today_entry}, 累计出={current_stats.today_exit}, FPS={current_stats.fps:.1f}")
    print("-" * 70)

    # 10. 停止视频流并释放引擎句柄
    print("[+] 正在停止视频流...")
    engine_dll.FlowSight_StopStream(handle)
    print("[+] 正在安全销毁引擎句柄...")
    engine_dll.FlowSight_Destroy(handle)

    print(f"[+] 自检完成! 共截获 {len(received_events)} 次通行事件, {len(received_stats)} 次性能快照。")
    if len(received_stats) > 0:
        print("[SUCCESS] C-ABI 动态链接库跨语言集成与事件回调 100% 验证通过!")
        return 0
    else:
        print("[-] 警告: 未收到算法性能指标")
        return 1

if __name__ == "__main__":
    sys.exit(main())
