package com.flowsight.cloud.dto;

import lombok.Data;
import java.util.List;

/**
 * 边缘端时间窗口聚合批量上报数据传输对象 (Batch Ingestion DTO)
 */
@Data
public class EdgeBatchReportDTO {

    /**
     * 批次唯一UUID (用于云端幂等去重与离线重传防重复计算)
     */
    private String batchUuid;

    /**
     * 客户端主机名/设备节点标识 (例如: FlowSight-Node-01)
     */
    private String clientName;

    /**
     * 客户端核心软件版本
     */
    private String appVersion;

    /**
     * 聚合时间窗口起始绝对毫秒时间戳
     */
    private Long startTimeMs;

    /**
     * 聚合时间窗口截止绝对毫秒时间戳
     */
    private Long endTimeMs;

    /**
     * 客流增量与实时人数指标
     */
    private HeadcountMetrics headcount;

    /**
     * 客群画像增量指标
     */
    private DemographicMetrics demographics;

    /**
     * 边缘设备硬件与系统健康遥测指标
     */
    private DeviceTelemetry telemetry;

    /**
     * 窗口期内发生的结构化通行事件精简记录列表
     */
    private List<EdgeEventDTO> events;

    @Data
    public static class HeadcountMetrics {
        private Integer inIncrement = 0;
        private Integer outIncrement = 0;
        private Integer currentOccupancy = 0;
    }

    @Data
    public static class DemographicMetrics {
        private Integer maleCount = 0;
        private Integer femaleCount = 0;
        private Integer childCount = 0;
        private Integer adultCount = 0;
        private Integer seniorCount = 0;
    }

    @Data
    public static class DeviceTelemetry {
        private Float fps = 0.0f;
        private Float inferLatencyMs = 0.0f;
        private Float cpuUsage = 0.0f;
        private Float memoryUsageMb = 0.0f;
        private String hardwareBackend = "CPU";
        private Boolean cameraConnected = true;
        private Integer pendingOfflineQueue = 0;
    }
}
