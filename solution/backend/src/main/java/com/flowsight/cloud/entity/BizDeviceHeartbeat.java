package com.flowsight.cloud.entity;

import com.baomidou.mybatisplus.annotation.IdType;
import com.baomidou.mybatisplus.annotation.TableId;
import com.baomidou.mybatisplus.annotation.TableName;
import lombok.Data;

import java.io.Serializable;
import java.time.LocalDateTime;

@Data
@TableName("biz_device_heartbeat")
public class BizDeviceHeartbeat implements Serializable {
    @TableId(type = IdType.INPUT)
    private Long pointId;
    private String clientName;
    private String clientVersion;
    private Float currentFps;
    private Float inferLatencyMs;
    private String hardwareBackend;
    private Integer currentOccupancy;
    private LocalDateTime lastHeartbeat;
}
