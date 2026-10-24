package com.flowsight.cloud.vo;

import lombok.Data;
import java.time.LocalDateTime;

@Data
public class UserVO {
    private Long id;
    private String phone;
    private String role;
    private String nickname;
    private String companyName;
    private String contactPerson;
    private Integer pointQuota;
    private LocalDateTime expireTime;
    private String avatar;
    private Integer status;
    private Integer pointCount;
    private LocalDateTime createTime;
}
