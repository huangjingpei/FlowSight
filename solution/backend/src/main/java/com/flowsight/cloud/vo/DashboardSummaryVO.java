package com.flowsight.cloud.vo;

import lombok.Data;
import java.util.List;

@Data
public class DashboardSummaryVO {
    private Long pointId;
    private String pointName;
    private String clientName;
    private String appVersion;
    private String lastHeartbeat;
    private String streamStatus;
    private int currentOccupancy;
    private int todayEntry;
    private int todayExit;
    private int todayTotal;
    private String snapshotUrl;
    private DemographicsVO demographics;
    private List<HourlyStatVO> hourlyChart;
}
