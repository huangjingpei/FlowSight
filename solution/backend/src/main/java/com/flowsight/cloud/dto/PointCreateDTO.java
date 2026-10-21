package com.flowsight.cloud.dto;

import jakarta.validation.constraints.NotBlank;
import lombok.Data;

@Data
public class PointCreateDTO {
    private Long userId; // 若管理员创建，需指定属主用户；若商户自建，取当前登录ID

    @NotBlank(message = "点位名称不能为空")
    private String pointName;

    private String address;

    private String cameraRtsp;

    private String resolution;
}
