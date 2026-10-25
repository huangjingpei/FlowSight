package com.flowsight.cloud.dto;

import jakarta.validation.constraints.NotBlank;
import lombok.Data;

@Data
public class PointCreateDTO {
    private Long userId; // 若管理员创建，需指定属主用户；若商户自建，取当前登录ID

    @NotBlank(message = "点位名称不能为空")
    private String pointName;

    private String pointCode; // 点位唯一编码 (若为空则由后台自动生成)

    private String devicePassword; // 边缘设备专属接入密码 (密码 2，默认 Edge@123456)

    private String address;

    private String cameraRtsp;

    private String resolution;

    private String bizType; // PEDESTRIAN / VEHICLE
}
