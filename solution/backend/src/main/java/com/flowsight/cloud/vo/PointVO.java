package com.flowsight.cloud.vo;

import lombok.Data;
import java.time.LocalDateTime;

@Data
public class PointVO {
    private Long id;
    private Long userId;
    private String userPhone;
    private String pointName;
    private String pointCode;
    private String address;
    private String cameraRtsp;
    private String resolution;
    private String streamStatus;
    private String pointToken;
    private Float currentFps;
    private Integer currentOccupancy;
    private LocalDateTime lastHeartbeat;
    private LocalDateTime createTime;
}
