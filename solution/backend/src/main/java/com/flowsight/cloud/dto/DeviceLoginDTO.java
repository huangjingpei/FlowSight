package com.flowsight.cloud.dto;

import jakarta.validation.constraints.NotBlank;
import lombok.Data;

@Data
public class DeviceLoginDTO {

    @NotBlank(message = "点位编码不能为空")
    private String pointCode;

    @NotBlank(message = "设备接入密码不能为空")
    private String devicePassword;

    private String clientName;

    private Boolean force;
}
