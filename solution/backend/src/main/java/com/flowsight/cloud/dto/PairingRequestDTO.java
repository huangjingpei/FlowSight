package com.flowsight.cloud.dto;

import jakarta.validation.constraints.NotBlank;
import lombok.Data;

@Data
public class PairingRequestDTO {

    @NotBlank(message = "6位设备配对码不能为空")
    private String pairingCode;

    private String clientName;
}
