package com.flowsight.cloud.dto;

import jakarta.validation.constraints.NotBlank;
import lombok.Data;

@Data
public class UserCreateDTO {
    @NotBlank(message = "手机号不能为空")
    private String phone;

    private String password; // 若为空，默认与手机号同号

    private String nickname;

    private String initialPointName; // 初始创建的点位名称 (选填)
}
