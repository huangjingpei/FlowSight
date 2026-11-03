package com.flowsight.cloud.vo;

import lombok.Data;
import java.util.List;

@Data
public class DashboardSummaryVO {
    private Long pointId;
    private String pointName;
    private String bizType; // PEDESTRIAN / VEHICLE
    private String clientName;
    private String appVersion;
    private String lastHeartbeat;
    private String streamStatus;
    private int currentOccupancy;
    private int todayEntry;
    private int todayExit;
    private int todayTotal;

    // 周期统计 (本月数/本年数/历年累计)
    private int monthTotal;
    private int yearTotal;
    private int historicalTotal;
    private String historicalStartDate;
    private String snapshotUrl;
    private DemographicsVO demographics;
    private List<HourlyStatVO> hourlyChart;
}
