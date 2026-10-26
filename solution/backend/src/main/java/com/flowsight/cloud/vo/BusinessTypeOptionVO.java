package com.flowsight.cloud.vo;

import lombok.AllArgsConstructor;
import lombok.Builder;
import lombok.Data;
import lombok.NoArgsConstructor;

import java.util.List;

@Data
@Builder
@NoArgsConstructor
@AllArgsConstructor
public class BusinessTypeOptionVO {
    private String bizCode;
    private String bizName;
    private String description;
    private List<String> featureList;
    private String deviceReq;
    private String icon;
    private String colorTag;
}
