package com.flowsight.cloud.vo;

import lombok.Data;

import java.time.LocalDateTime;
import java.util.List;

@Data
public class BusinessTypeVO {
    private Long id;
    private String bizCode;
    private String bizName;
    private String description;
    private String features;          // 原始 JSON 存储文本
    private List<String> featureList; // 解析后的特征属性标签集合
    private String deviceReq;
    private String icon;
    private String colorTag;
    private Integer status;
    private Integer sortOrder;
    private Integer isSystem;
    private Integer pointCount;       // 关联绑定的有效点位数
    private LocalDateTime createTime;
    private LocalDateTime updateTime;
}
