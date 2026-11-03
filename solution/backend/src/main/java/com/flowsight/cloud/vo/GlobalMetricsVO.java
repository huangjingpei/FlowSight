package com.flowsight.cloud.vo;

import lombok.Data;

@Data
public class GlobalMetricsVO {
    private int totalPoints;
    private int onlinePoints;
    private int offlinePoints;
    private int pedestrianPoints;
    private int vehiclePoints;
    
    // 客流全局聚合
    private int totalPedestrianOccupancy;
    private int totalPedestrianEntry;
    private int totalPedestrianExit;
    private int totalPedestrianMonth;
    private int totalPedestrianYear;
    private int totalPedestrianHistorical;

    // 车流全局聚合
    private int totalVehicleOccupancy;
    private int totalVehicleEntry;
    private int totalVehicleExit;
    private int totalVehicleMonth;
    private int totalVehicleYear;
    private int totalVehicleHistorical;

    // 历年统计起始日期基准 (默认 "2027年1月1日")
    private String historicalStartDate;
}
