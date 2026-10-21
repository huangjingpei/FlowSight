package com.flowsight.cloud.dto;

import lombok.Data;

@Data
public class PointUpdateDTO {
    private String pointName;
    private String address;
    private String cameraRtsp;
    private String resolution;
    private String streamStatus;
}
