package com.flowsight.cloud.entity;

import com.baomidou.mybatisplus.annotation.IdType;
import com.baomidou.mybatisplus.annotation.TableId;
import com.baomidou.mybatisplus.annotation.TableName;
import lombok.Data;

import java.io.Serializable;
import java.time.LocalDate;
import java.time.LocalDateTime;

@Data
@TableName("biz_stats_hourly")
public class BizStatsHourly implements Serializable {
    @TableId(type = IdType.AUTO)
    private Long id;
    private Long pointId;
    private Long userId;
    private LocalDate statDate;
    private Integer statHour; // 0 ~ 23
    private Integer entryCount;
    private Integer exitCount;
    private Integer maleCount;
    private Integer femaleCount;
    private LocalDateTime updateTime;
}
