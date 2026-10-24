package com.flowsight.cloud.dto;

import com.fasterxml.jackson.annotation.JsonFormat;
import jakarta.validation.constraints.NotBlank;
import lombok.Data;

import java.time.LocalDateTime;

@Data
public class UserCreateDTO {
    @NotBlank(message = "手机号不能为空")
    private String phone;

    private String password; // 若为空，默认与手机号同号

    private String nickname;

    private String companyName;

    private String contactPerson;

    private Integer pointQuota; // 默认 5

    @JsonFormat(pattern = "yyyy-MM-dd HH:mm:ss")
    private LocalDateTime expireTime;

    private String initialPointName; // 初始创建的点位名称 (选填)
}
