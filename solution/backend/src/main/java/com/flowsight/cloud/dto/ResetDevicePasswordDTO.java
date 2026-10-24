package com.flowsight.cloud.dto;

import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.Size;
import lombok.Data;

@Data
public class ResetDevicePasswordDTO {

    @NotBlank(message = "新设备专属接入密码不能为空")
    @Size(min = 6, message = "设备密码长度不能少于6位")
    private String newDevicePassword;
}
