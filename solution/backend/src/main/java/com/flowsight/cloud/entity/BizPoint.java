package com.flowsight.cloud.entity;

import com.baomidou.mybatisplus.annotation.IdType;
import com.baomidou.mybatisplus.annotation.TableId;
import com.baomidou.mybatisplus.annotation.TableLogic;
import com.baomidou.mybatisplus.annotation.TableName;
import lombok.Data;

import java.io.Serializable;
import java.time.LocalDateTime;

@Data
@TableName("biz_point")
public class BizPoint implements Serializable {
    @TableId(type = IdType.AUTO)
    private Long id;
    private Long userId;
    private String pointName;
    private String pointCode;
    private String address;
    private String cameraRtsp;
    private String resolution;
    private String streamStatus; // ONLINE, OFFLINE, ALERT
    private String pointToken;
    private String devicePasswordHash;
    
    @TableLogic
    private Integer isDeleted;
    
    private LocalDateTime createTime;
    private LocalDateTime updateTime;
}
