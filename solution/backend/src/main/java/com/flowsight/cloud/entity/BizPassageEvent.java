package com.flowsight.cloud.entity;

import com.baomidou.mybatisplus.annotation.IdType;
import com.baomidou.mybatisplus.annotation.TableId;
import com.baomidou.mybatisplus.annotation.TableName;
import lombok.Data;

import java.io.Serializable;
import java.time.LocalDateTime;

@Data
@TableName("biz_passage_event")
public class BizPassageEvent implements Serializable {
    @TableId(type = IdType.AUTO)
    private Long id;
    private Long pointId;
    private Long userId;
    private Integer trackId;
    private String direction; // ENTRY, EXIT
    private Float confidence;
    private String gender; // 男, 女, 未知
    private String ageGroup; // 青年(18-35) 等
    private String plateNumber; // 车牌号: 浙A·88888
    private String plateColor; // 蓝色, 绿色, 黄色, 白色, 其他
    private String vehicleType; // 小型客车, 大型客车, 载货汽车等
    private String vehicleColor; // 白色, 黑色, 灰色, 红色, 蓝色等
    private Long timestampMs;
    private String clipUrl;
    private String localClipPath;
    private LocalDateTime createTime;
}
