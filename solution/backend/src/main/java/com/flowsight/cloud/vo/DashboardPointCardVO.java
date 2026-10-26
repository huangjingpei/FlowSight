package com.flowsight.cloud.vo;

import com.fasterxml.jackson.annotation.JsonProperty;
import lombok.Data;

@Data
public class DashboardPointCardVO {
    private Long pointId;
    private String pointName;
    private String pointCode;
    private String bizType; // PEDESTRIAN / VEHICLE / CUSTOM...
    private String bizTypeName; // 友好名称
    private String streamStatus; // ONLINE / OFFLINE
    private int currentOccupancy;
    private int todayEntry;
    private int todayExit;
    private int todayTotal;
    private String snapshotUrl;

    @JsonProperty("isDefault")
    private boolean isDefault;

    private String lastHeartbeat;
    private String clientName;
}
