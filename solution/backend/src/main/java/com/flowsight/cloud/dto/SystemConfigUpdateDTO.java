package com.flowsight.cloud.dto;

import jakarta.validation.constraints.NotBlank;
import lombok.Data;

@Data
public class SystemConfigUpdateDTO {
    @NotBlank(message = "配置标识键名不能为空")
    private String configKey;

    @NotBlank(message = "配置值不能为空")
    private String configValue;
}
