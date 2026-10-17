#pragma once

/**
 * @file FlowSightAPI.h
 * @brief FlowSight 核心引擎公共 C++ 抽象接口与解耦 DTO 规范
 * @note 本头文件为对外公开接口，严禁引入任何第三方库头文件 (如 OpenCV, Qt, ONNX Runtime 等)。
 *       所有数据类型均为标准 C++ 基础类型 (POD 结构与标准库)，保障跨平台、跨语言调用的 ABI 稳定性。
 */

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace flowsight {

#if defined(_WIN32)
    #if defined(FLOWSIGHT_BUILD_SHARED)
        #define FLOWSIGHT_API __declspec(dllexport)
    #else
        #define FLOWSIGHT_API
    #endif
#else
    #define FLOWSIGHT_API __attribute__((visibility("default")))
#endif

// ============================================================================
// 1. 基础数据类型 (纯标准 C++ POD 几何对象)
// ============================================================================

/**
 * @brief 2D 浮点像素坐标点 (解耦 cv::Point2f)
 */
struct Point2D {
    float x{0.0f};
    float y{0.0f};
};

/**
 * @brief 2D 浮点矩形外接框 (解耦 cv::Rect2f)
 */
struct Rect2D {
    float x{0.0f};
    float y{0.0f};
    float width{0.0f};
    float height{0.0f};
};

/**
 * @brief 空间任意 N 边形感知区域定义
 */
struct PolygonRegion {
    int32_t              channel_id{0};       ///< 关联的通道 ID (默认为 0)
    std::string          region_id;           ///< 区域唯一标识 (如 "corridor_gate_01")
    std::string          region_name;         ///< 人类可读名称 (如 "南门主通道")
    std::vector<Point2D> vertex_points;       ///< 多边形顶点数组 (N >= 3, 顺时针或逆时针闭合)
};

/**
 * @brief 客流穿行方向枚举
 */
enum class Direction : uint8_t {
    UNKNOWN = 0, ///< 未知/未完成穿行
    ENTRY   = 1, ///< 进入 (穿入指定区域内部)
    EXIT    = 2  ///< 离开 (穿出指定区域外部)
};

// ============================================================================
// 2. 纯数据传输对象 (DTO): 事件与指标输出载荷
// ============================================================================

/**
 * @brief 经过防抖与去重确认的标准客流通行事件 DTO
 */
struct FlowEventDTO {
    int32_t     channel_id{0};       ///< [前瞻字段] 区分是哪一路摄像机触发的事件 (默认 0)
    std::string camera_id{"CAM_01"}; ///< [前瞻字段] 摄像机全局业务编码
    int64_t     event_id{0};         ///< 全局事件自增 ID
    int64_t     timestamp_ms{0};     ///< 事件触发 Unix 毫秒时间戳
    int32_t     track_id{-1};        ///< 目标在生命周期内的唯一跟踪轨迹 ID
    Direction   direction{Direction::UNKNOWN}; ///< 确认为进入还是离开
    std::string region_id;           ///< 触发该事件的区域标识
    float       confidence{0.0f};    ///< 算法判定置信度 (0.0 ~ 1.0)
    Point2D     foot_location;       ///< 穿行确认时刻足底着地点坐标
    std::string clip_file_path;      ///< 关联录制的取证 MP4 切片路径 (若开启)
};

/**
 * @brief 系统实时吞吐与健康指标快照 DTO
 */
struct SystemStatsDTO {
    int32_t     channel_id{0};       ///< [前瞻字段] 关联通道 ID
    std::string camera_id{"CAM_01"}; ///< [前瞻字段] 关联摄像机业务标识
    int32_t     current_occupancy{0};    ///< 当前区域内活跃/滞留人数
    int32_t     total_entry_count{0};    ///< 今日累计进入人次 (去重后)
    int32_t     total_exit_count{0};     ///< 今日累计离开人次 (去重后)
    float       inference_latency_ms{0}; ///< 神经网络前向推理单帧耗时 (ms)
    float       pipeline_fps{0.0f};      ///< 主算法处理管线综合实时帧率
};

/**
 * @brief 通道配置参数 (针对多路摄像机并发前瞻设计)
 */
struct ChannelConfig {
    int32_t       channel_id{0};           ///< 通道数字编号
    std::string   camera_id{"CAM_01"};     ///< 摄像机全局业务标签
    std::string   video_uri;               ///< 视频源路径 (本地 MP4 或 RTSP URL)
    PolygonRegion region;                  ///< 该通道关联的空间多边形
    bool          enable_recording{true};  ///< 是否开启短视频切片录制
};

/**
 * @brief 通道实时运行状态快照
 */
struct ChannelStatus {
    int32_t     channel_id{0};
    std::string camera_id;
    bool        is_active{false};
    float       current_fps{0.0f};
    int32_t     today_entry{0};
    int32_t     today_exit{0};
};

// ============================================================================
// 3. 强类型系统解耦回调签名
// ============================================================================

/**
 * @brief 客流通行事件回调函数类型
 * @param event 已经过空间迟滞带与轨迹生命周期去重确认的事件 DTO
 * @param user_context 用户自定义上下文指针 (如外部系统的 this 指针或句柄)
 */
using FlowEventCallback = std::function<void(const FlowEventDTO& event, void* user_context)>;

/**
 * @brief 实时性能指标与状态回调函数类型
 */
using SystemStatsCallback = std::function<void(const SystemStatsDTO& stats, void* user_context)>;

// ============================================================================
// 4. 纯虚核心引擎接口 (PIMPL 抽象层)
// ============================================================================

class IFlowSightEngine {
public:
    virtual ~IFlowSightEngine() = default;

    /**
     * @brief 初始化核心推理与跟踪引擎
     * @param model_path 模型权重路径 (ONNX 或 RKNN)
     * @param conf_thresh 检测置信度阈值 (默认 0.45)
     * @param device_id 计算设备编号 (0=GPU/NPU, -1=CPU)
     */
    virtual bool initialize(const std::string& model_path, float conf_thresh = 0.45f, int device_id = 0) = 0;

    /**
     * @brief 注册去重后的通行事件系统回调
     */
    virtual void register_event_callback(FlowEventCallback callback, void* user_context = nullptr) = 0;

    /**
     * @brief 注册系统状态与性能指标回调
     */
    virtual void register_stats_callback(SystemStatsCallback callback, void* user_context = nullptr) = 0;

    // ------------------------------------------------------------------------
    // 单通道便捷接口 (向前兼容当前 MVP Demo)
    // ------------------------------------------------------------------------

    /**
     * @brief 设置或动态更新空间感知多边形 (单路便捷接口)
     */
    virtual bool set_detection_region(const PolygonRegion& region) = 0;

    /**
     * @brief 启动流处理 (支持本地标准 MP4 文件路径或 RTSP 实时流)
     */
    virtual bool start_stream(const std::string& video_uri) = 0;

    /**
     * @brief 停止流处理并释放管线资源
     */
    virtual void stop_stream() = 0;

    // ------------------------------------------------------------------------
    // 多通道前瞻扩展接口 (v2.0 架构预留，满足多路摄像机平滑演进与性能压测)
    // ------------------------------------------------------------------------

    /**
     * @brief 注册添加新的视频分析通道 (多路扩展)
     * @return 返回分配的 channel_id，小于 0 表示失败
     */
    virtual int32_t add_channel(const ChannelConfig& config) = 0;

    /**
     * @brief 移除指定视频通道
     */
    virtual bool remove_channel(int32_t channel_id) = 0;

    /**
     * @brief 启动指定通道分析管线
     */
    virtual bool start_channel(int32_t channel_id) = 0;

    /**
     * @brief 停止指定通道分析管线
     */
    virtual void stop_channel(int32_t channel_id) = 0;

    /**
     * @brief 动态更新指定通道的多边形感知区域
     */
    virtual bool set_channel_region(int32_t channel_id, const PolygonRegion& region) = 0;

    /**
     * @brief 获取所有视频通道的实时运行状态
     */
    virtual std::vector<ChannelStatus> get_all_channels_status() const = 0;
};

// ============================================================================
// 5. C 风格导出工厂 (便于跨语言绑定与 SDK 封装)
// ============================================================================

extern "C" {
    FLOWSIGHT_API IFlowSightEngine* CreateFlowSightEngine();
    FLOWSIGHT_API void DestroyFlowSightEngine(IFlowSightEngine* engine);
}

} // namespace flowsight
