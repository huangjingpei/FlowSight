package com.flowsight.cloud.dto;

import lombok.Data;

@Data
public class PointUpdateDTO {
    private String pointName;
    private String pointCode;
    private String devicePassword;
    private String address;
    private String cameraRtsp;
    private String resolution;
    private String bizType;
    private String streamStatus;
}
