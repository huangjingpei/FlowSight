package com.flowsight.cloud.dto;

import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.Pattern;
import lombok.Data;

import java.util.List;

@Data
public class BusinessTypeDTO {
    @NotBlank(message = "业务英文标识编码不能为空")
    @Pattern(regexp = "^[A-Z0-9_]{2,32}$", message = "业务编码只能包含大写字母、数字和下划线，长度2-32位")
    private String bizCode;

    @NotBlank(message = "业务模式名称不能为空")
    private String bizName;

    private String description;

    /**
     * 前端传入的 AI 特性属性标签列表 (例如: ["人体检测", "性别分析", "年龄段识别"])
     */
    private List<String> featureList;

    private String deviceReq;
    private String icon;
    private String colorTag;
    private Integer status;
    private Integer sortOrder;
}
