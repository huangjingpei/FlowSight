package com.flowsight.cloud.vo;

import lombok.Data;

@Data
public class LoginVO {
    private String token;
    private Long userId;
    private String role;
    private String phone;
    private String nickname;
    private String companyName;
    private String avatar;
}
