package com.flowsight.cloud.dto;

import lombok.Data;

@Data
public class UserUpdateDTO {
    private String nickname;
    private String password;
    private Integer status; // 1-正常, 0-禁用
}
