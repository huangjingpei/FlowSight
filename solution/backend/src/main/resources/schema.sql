-- FlowSight Cloud Backend Database Initialization Script (MySQL 8.0)

-- 1. 用户认证与租户表
CREATE TABLE IF NOT EXISTS `sys_user` (
    `id` BIGINT NOT NULL AUTO_INCREMENT COMMENT '用户主键ID',
    `phone` VARCHAR(20) NOT NULL COMMENT '登录手机号码(唯一凭证)',
    `password_hash` VARCHAR(128) NOT NULL COMMENT 'BCrypt 加密密码散列',
    `role` VARCHAR(20) NOT NULL DEFAULT 'ROLE_USER' COMMENT '角色标识: ROLE_ADMIN / ROLE_USER',
    `nickname` VARCHAR(64) NOT NULL DEFAULT '' COMMENT '用户或商户展示名称',
    `avatar` VARCHAR(255) DEFAULT '' COMMENT '用户头像地址',
    `status` TINYINT NOT NULL DEFAULT 1 COMMENT '账号状态: 1-正常, 0-禁用',
    `create_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '注册创建时间',
    `update_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '最后更新时间',
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_phone` (`phone`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='用户与租户基础表';

-- 预置超级管理员账号 (手机号: 13800000000, 密码与手机同号: 13800000000, BCrypt hash)
INSERT INTO `sys_user` (`phone`, `password_hash`, `role`, `nickname`) 
VALUES ('13800000000', '$2a$10$7JB720yubVSZvUI0rEqK/.Vq4VfH4FjPvhV7r0h1hA9x.VcmQYVce', 'ROLE_ADMIN', '系统总管理员')
ON DUPLICATE KEY UPDATE `id`=`id`;

-- 2. 点位资产配置表
CREATE TABLE IF NOT EXISTS `biz_point` (
    `id` BIGINT NOT NULL AUTO_INCREMENT COMMENT '点位主键ID',
    `user_id` BIGINT NOT NULL COMMENT '所属商户用户ID',
    `point_name` VARCHAR(100) NOT NULL COMMENT '点位名称(例如: A门主入口)',
    `point_code` VARCHAR(64) NOT NULL COMMENT '点位唯一编码',
    `address` VARCHAR(255) DEFAULT '' COMMENT '物理安装具体位置/门店地址',
    `camera_rtsp` VARCHAR(512) DEFAULT '' COMMENT '摄像头 RTSP 流地址',
    `resolution` VARCHAR(32) DEFAULT '1920x1080' COMMENT '推流画面分辨率规格',
    `stream_status` VARCHAR(20) NOT NULL DEFAULT 'OFFLINE' COMMENT '状态: ONLINE-在线, OFFLINE-离线, ALERT-异常',
    `point_token` VARCHAR(64) NOT NULL COMMENT '边缘采集端专用对接认证Token',
    `is_deleted` TINYINT NOT NULL DEFAULT 0 COMMENT '逻辑删除标记: 0-正常, 1-已删除',
    `create_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    `update_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '配置更新时间',
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_point_code` (`point_code`),
    KEY `idx_user_id` (`user_id`),
    KEY `idx_token` (`point_token`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='监控分析点位资产表';

-- 3. 人流通行结构化事件流水明细表
CREATE TABLE IF NOT EXISTS `biz_passage_event` (
    `id` BIGINT NOT NULL AUTO_INCREMENT COMMENT '事件全局唯一流水ID',
    `point_id` BIGINT NOT NULL COMMENT '所属点位ID',
    `user_id` BIGINT NOT NULL COMMENT '所属商户用户ID(反范式提速查询)',
    `track_id` INT NOT NULL COMMENT '跟踪目标局部ID',
    `direction` VARCHAR(16) NOT NULL COMMENT '通行判定方向: ENTRY-进入, EXIT-离开',
    `confidence` FLOAT NOT NULL DEFAULT 0.0 COMMENT '检测综合置信度',
    `gender` VARCHAR(16) DEFAULT '未知' COMMENT '客群画像预测性别: 男/女/未知',
    `age_group` VARCHAR(32) DEFAULT '未知' COMMENT '预测年龄段: 青年(18-35)等',
    `timestamp_ms` BIGINT NOT NULL COMMENT '通行发生绝对毫秒时间戳',
    `clip_url` VARCHAR(512) DEFAULT '' COMMENT '云端短视频切片 MinIO 访问路径',
    `local_clip_path` VARCHAR(255) DEFAULT '' COMMENT '边缘端原始生成切片相对路径',
    `create_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '事件入库时间',
    PRIMARY KEY (`id`),
    KEY `idx_point_time` (`point_id`, `timestamp_ms`),
    KEY `idx_user_time` (`user_id`, `timestamp_ms`),
    KEY `idx_track` (`track_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='通行事件明细流水表';

-- 4. 小时级商业指标预聚合表
CREATE TABLE IF NOT EXISTS `biz_stats_hourly` (
    `id` BIGINT NOT NULL AUTO_INCREMENT COMMENT '自增主键',
    `point_id` BIGINT NOT NULL COMMENT '点位ID',
    `user_id` BIGINT NOT NULL COMMENT '用户ID',
    `stat_date` DATE NOT NULL COMMENT '统计日期(yyyy-MM-dd)',
    `stat_hour` TINYINT NOT NULL COMMENT '统计小时段(0~23)',
    `entry_count` INT NOT NULL DEFAULT 0 COMMENT '小时累计进入人数',
    `exit_count` INT NOT NULL DEFAULT 0 COMMENT '小时累计离开人数',
    `male_count` INT NOT NULL DEFAULT 0 COMMENT '男性顾客人次',
    `female_count` INT NOT NULL DEFAULT 0 COMMENT '女性顾客人次',
    `update_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_point_date_hour` (`point_id`, `stat_date`, `stat_hour`),
    KEY `idx_user_date` (`user_id`, `stat_date`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='小时级客流画像预聚合表';

-- 5. 边缘设备心跳与分析健康状态表
CREATE TABLE IF NOT EXISTS `biz_device_heartbeat` (
    `point_id` BIGINT NOT NULL COMMENT '关联点位ID',
    `client_name` VARCHAR(64) DEFAULT 'FlowSight-Node' COMMENT '采集端节点/机器名称',
    `client_version` VARCHAR(32) NOT NULL DEFAULT 'v1.2.0' COMMENT '采集端核心软件版本',
    `current_fps` FLOAT NOT NULL DEFAULT 0.0 COMMENT '边缘当前处理帧率',
    `infer_latency_ms` FLOAT NOT NULL DEFAULT 0.0 COMMENT '单帧平均推理延迟毫秒',
    `hardware_backend` VARCHAR(32) DEFAULT 'CPU' COMMENT '运行硬件后端(CUDA/CPU/RKNN)',
    `current_occupancy` INT NOT NULL DEFAULT 0 COMMENT '当前防区滞留人数',
    `last_heartbeat` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '最新心跳时间',
    PRIMARY KEY (`point_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='边缘设备健康心跳表';
