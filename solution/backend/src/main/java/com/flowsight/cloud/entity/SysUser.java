package com.flowsight.cloud.entity;

import com.baomidou.mybatisplus.annotation.IdType;
import com.baomidou.mybatisplus.annotation.TableId;
import com.baomidou.mybatisplus.annotation.TableName;
import lombok.Data;

import java.io.Serializable;
import java.time.LocalDateTime;

@Data
@TableName("sys_user")
public class SysUser implements Serializable {
    @TableId(type = IdType.AUTO)
    private Long id;
    private String phone;
    private String passwordHash;
    private String role; // ROLE_ADMIN, ROLE_USER
    private String nickname;
    private String avatar;
    private Integer status; // 1-正常, 0-禁用
    private LocalDateTime createTime;
    private LocalDateTime updateTime;
}
