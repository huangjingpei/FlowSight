package com.flowsight.cloud.entity;

import com.baomidou.mybatisplus.annotation.IdType;
import com.baomidou.mybatisplus.annotation.TableId;
import com.baomidou.mybatisplus.annotation.TableName;
import lombok.Data;

import java.io.Serializable;
import java.time.LocalDateTime;

/**
 * 业务模型与感知业务类型配置实体 (与点位 biz_type 关联)
 */
@Data
@TableName("sys_business_type")
public class BusinessType implements Serializable {
    @TableId(type = IdType.AUTO)
    private Long id;

    /**
     * 业务模式唯一英文标识 (与边缘端算法协议及点位表 biz_type 一致, 如 PEDESTRIAN, VEHICLE)
     */
    private String bizCode;

    /**
     * 业务模式展示名称 (如: 客流检测与客群分析, 车流检测与车辆分析)
     */
    private String bizName;

    /**
     * 业务定位场景与部署要求描述
     */
    private String description;

    /**
     * 支持的 AI 算法特征属性列表 (JSON 字符串存储, 如 ["人体检测","进出计数","性别分析","年龄段预测"])
     */
    private String features;

    /**
     * 前端采集与硬件算力要求建议 (如推荐分辨率/帧率/架设角度)
     */
    private String deviceReq;

    /**
     * 前端 UI 展示图标 (如 User, Van, VideoCamera)
     */
    private String icon;

    /**
     * 标签色彩十六进制值 (如 #388bfd, #e3b341)
     */
    private String colorTag;

    /**
     * 业务启用状态: 1-启用, 0-停用
     */
    private Integer status;

    /**
     * 显示排序权重 (升序)
     */
    private Integer sortOrder;

    /**
     * 是否系统预置: 1-系统内置(禁止删除), 0-自定义扩展业务
     */
    private Integer isSystem;

    private LocalDateTime createTime;
    private LocalDateTime updateTime;
}
