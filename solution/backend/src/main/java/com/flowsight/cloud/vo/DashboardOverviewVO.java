package com.flowsight.cloud.vo;

import lombok.Data;
import java.util.List;

@Data
public class DashboardOverviewVO {
    private Long defaultPointId;
    private GlobalMetricsVO globalMetrics;
    private List<DashboardPointCardVO> pointCards;
}
