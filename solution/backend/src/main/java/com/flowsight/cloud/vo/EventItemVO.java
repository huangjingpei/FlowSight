package com.flowsight.cloud.vo;

import lombok.Data;

@Data
public class EventItemVO {
    private Long id;
    private Long pointId;
    private String pointName;
    private Integer trackId;
    private String direction;
    private Float confidence;
    private String gender;
    private String ageGroup;
    private Long timestampMs;
    private String datetime;
    private String playUrl; // MinIO 临时签名/直链播放地址
}
