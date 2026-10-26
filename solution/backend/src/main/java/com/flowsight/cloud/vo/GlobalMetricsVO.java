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

    // 车流全局聚合
    private int totalVehicleOccupancy;
    private int totalVehicleEntry;
    private int totalVehicleExit;
}
