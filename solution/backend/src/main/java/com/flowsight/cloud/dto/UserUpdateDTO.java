package com.flowsight.cloud.dto;

import com.fasterxml.jackson.annotation.JsonFormat;
import lombok.Data;

import java.time.LocalDateTime;

@Data
public class UserUpdateDTO {
    private String nickname;
    private String password;
    private Integer status; // 1-正常, 0-禁用
    private String companyName;
    private String contactPerson;
    private Integer pointQuota;
    @JsonFormat(pattern = "yyyy-MM-dd HH:mm:ss")
    private LocalDateTime expireTime;
}
