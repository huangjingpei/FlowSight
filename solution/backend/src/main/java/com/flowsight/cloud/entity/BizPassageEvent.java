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
    private Long timestampMs;
    private String clipUrl;
    private String localClipPath;
    private LocalDateTime createTime;
}
