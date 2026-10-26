package com.flowsight.cloud.vo;

import lombok.Data;
import java.util.List;

/**
 * 多维度客流趋势分析报表 VO (按小时、按天、按季度)
 * 适配政务与物业管理端高保真曲线图与决策大屏展示
 */
@Data
public class TrendReportVO {
    /**
     * 当前统计维度: hour(逐时), day(逐日), quarter(逐季)
     */
    private String dimension;

    /**
     * 业务模式: PEDESTRIAN-客流, VEHICLE-车流
     */
    private String bizType = "PEDESTRIAN";

    /**
     * 维度标题 (例如: "今日 24 小时通行曲线", "近 7 天日均通行趋势", "年度季度客流走势")
     */
    private String title;

    /**
     * X 轴分类标签集合 (例如: ["08:00", "09:00"...] 或 ["10-18", "10-19"...] 或 ["2026-Q1", "2026-Q2"...])
     */
    private List<String> categories;

    /**
     * 进入人次数值序列
     */
    private List<Integer> entryList;

    /**
     * 离开人次数值序列
     */
    private List<Integer> exitList;

    /**
     * 双向总通行人次序列
     */
    private List<Integer> totalList;

    /**
     * 周期内最高单点峰值数值
     */
    private Integer peakValue = 0;

    /**
     * 峰值产生的时间点或日期 (例如: "14:00" 或 "2026-10-21")
     */
    private String peakTime = "-";

    /**
     * 周期内全量累计客流
     */
    private Integer totalPeriodCount = 0;

    /**
     * 周期内平均客流 (例如: 逐日时为日均，逐时为时均)
     */
    private Double averageCount = 0.0;
}
