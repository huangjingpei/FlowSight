package com.flowsight.cloud.vo;

import lombok.Data;

@Data
public class DemographicsVO {
    private int maleCount;
    private int femaleCount;
    private double maleRatio;
    private double femaleRatio;
    private String topAgeGroup;
}
