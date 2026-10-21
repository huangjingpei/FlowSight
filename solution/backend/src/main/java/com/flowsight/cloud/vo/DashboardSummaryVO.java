package com.flowsight.cloud.vo;

import lombok.Data;
import java.util.List;

@Data
public class DashboardSummaryVO {
    private Long pointId;
    private String pointName;
    private String streamStatus;
    private int currentOccupancy;
    private int todayEntry;
    private int todayExit;
    private int todayTotal;
    private DemographicsVO demographics;
    private List<HourlyStatVO> hourlyChart;
}
