package com.flowsight.cloud.vo;

import lombok.AllArgsConstructor;
import lombok.Builder;
import lombok.Data;
import lombok.NoArgsConstructor;

@Data
@Builder
@NoArgsConstructor
@AllArgsConstructor
public class DeviceLoginVO {

    private Long pointId;
    private Long userId;
    private String pointCode;
    private String pointName;
    private String pointToken;
    private String cameraRtsp;
    private String resolution;
    private String bizType;
    private String clientName;
}
