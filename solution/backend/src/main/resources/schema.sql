-- FlowSight Cloud Backend Database Initialization Script (MySQL 8.0)
-- B2B 多租户边云协同智能视觉分析 SaaS 平台数据定义与初始种子管理员

-- 1. 用户认证与多租户客户表
CREATE TABLE IF NOT EXISTS `sys_user` (
    `id` BIGINT NOT NULL AUTO_INCREMENT COMMENT '用户主键ID',
    `phone` VARCHAR(20) NOT NULL COMMENT '登录手机号码(唯一凭证)',
    `password_hash` VARCHAR(128) NOT NULL COMMENT 'BCrypt 加密密码散列',
    `role` VARCHAR(20) NOT NULL DEFAULT 'ROLE_USER' COMMENT '角色标识: ROLE_ADMIN(平台超管) / ROLE_USER(商户客户)',
    `nickname` VARCHAR(64) NOT NULL DEFAULT '' COMMENT '用户或商户简称',
    `company_name` VARCHAR(128) NOT NULL DEFAULT '' COMMENT '商户单位/企业客户全称',
    `contact_person` VARCHAR(64) NOT NULL DEFAULT '' COMMENT '客户责任联系人姓名',
    `point_quota` INT NOT NULL DEFAULT 5 COMMENT '允许开通监控点位配额上限',
    `expire_time` DATETIME DEFAULT NULL COMMENT 'SaaS订阅授权到期时间',
    `avatar` VARCHAR(255) DEFAULT '' COMMENT '用户头像地址',
    `status` TINYINT NOT NULL DEFAULT 1 COMMENT '账号状态: 1-正常, 0-禁用',
    `create_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '注册创建时间',
    `update_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '最后更新时间',
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_phone` (`phone`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='用户与多租户商户基础表';

-- 预置种子账号: 仅保留超级管理员账号 (手机号: 13800000000, 密码同号: 13800000000)
INSERT INTO `sys_user` (`id`, `phone`, `password_hash`, `role`, `nickname`, `company_name`, `contact_person`, `point_quota`, `expire_time`) 
VALUES (1, '13800000000', '$2a$10$7JB720yubVSZvUI0rEqK/.Vq4VfH4FjPvhV7r0h1hA9x.VcmQYVce', 'ROLE_ADMIN', '系统管理员', 'FlowSight SaaS 运营指挥中心', '系统运维部', 999, '2099-12-31 23:59:59')
ON DUPLICATE KEY UPDATE `company_name`=VALUES(`company_name`), `contact_person`=VALUES(`contact_person`), `point_quota`=VALUES(`point_quota`);

-- 2. 点位资产配置表 (包含业务模式 biz_type 字段，原生定义，无 ALTER TABLE)
CREATE TABLE IF NOT EXISTS `biz_point` (
    `id` BIGINT NOT NULL AUTO_INCREMENT COMMENT '点位主键ID',
    `user_id` BIGINT NOT NULL COMMENT '所属商户客户ID',
    `point_name` VARCHAR(100) NOT NULL COMMENT '点位名称(例如: A门主入口)',
    `point_code` VARCHAR(64) NOT NULL COMMENT '点位唯一编码',
    `address` VARCHAR(255) DEFAULT '' COMMENT '物理安装具体位置/门店地址',
    `camera_rtsp` VARCHAR(512) DEFAULT '' COMMENT '摄像头 RTSP 流地址',
    `resolution` VARCHAR(32) DEFAULT '1920x1080' COMMENT '推流画面分辨率规格',
    `biz_type` VARCHAR(16) NOT NULL DEFAULT 'PEDESTRIAN' COMMENT '业务模式: PEDESTRIAN-客流, VEHICLE-车流',
    `stream_status` VARCHAR(20) NOT NULL DEFAULT 'OFFLINE' COMMENT '状态: ONLINE-在线, OFFLINE-离线, ALERT-异常',
    `point_token` VARCHAR(64) NOT NULL COMMENT '边缘采集端专用对接认证Token',
    `device_password_hash` VARCHAR(128) NOT NULL DEFAULT '' COMMENT '边缘专属设备接入密码(BCrypt散列)',
    `snapshot_url` VARCHAR(512) DEFAULT '' COMMENT '现场实时画面快照预览地址',
    `is_deleted` TINYINT NOT NULL DEFAULT 0 COMMENT '逻辑删除标记: 0-正常, 1-已删除',
    `create_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    `update_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '配置更新时间',
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_point_code` (`point_code`),
    KEY `idx_user_id` (`user_id`),
    KEY `idx_token` (`point_token`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='监控分析点位资产表';

-- 3. 通行结构化事件流水明细表 (原生包含客流属性与车流车牌属性，无需任何 ALTER TABLE)
CREATE TABLE IF NOT EXISTS `biz_passage_event` (
    `id` BIGINT NOT NULL AUTO_INCREMENT COMMENT '事件全局唯一流水ID',
    `point_id` BIGINT NOT NULL COMMENT '所属点位ID',
    `user_id` BIGINT NOT NULL COMMENT '所属商户用户ID(反范式提速查询)',
    `track_id` INT NOT NULL COMMENT '跟踪目标局部ID',
    `direction` VARCHAR(16) NOT NULL COMMENT '通行判定方向: ENTRY-进入, EXIT-离开',
    `confidence` FLOAT NOT NULL DEFAULT 0.0 COMMENT '检测综合置信度',
    `gender` VARCHAR(16) DEFAULT '未知' COMMENT '客群画像预测性别: 男/女/未知',
    `age_group` VARCHAR(32) DEFAULT '未知' COMMENT '预测年龄段: 青年(18-35)等',
    `plate_number` VARCHAR(32) DEFAULT '' COMMENT '车牌号码: 浙A·88888',
    `plate_color` VARCHAR(16) DEFAULT '' COMMENT '号牌颜色: 蓝色/绿色/黄色/白色/其他',
    `vehicle_type` VARCHAR(32) DEFAULT '' COMMENT '车辆类型: 小型客车/大型客车/载货汽车等',
    `vehicle_color` VARCHAR(16) DEFAULT '' COMMENT '车身颜色: 白色/黑色/灰色/红色/蓝色等',
    `timestamp_ms` BIGINT NOT NULL COMMENT '通行发生绝对毫秒时间戳',
    `clip_url` VARCHAR(512) DEFAULT '' COMMENT '云端短视频切片 MinIO 访问路径',
    `local_clip_path` VARCHAR(255) DEFAULT '' COMMENT '边缘端原始生成切片相对路径',
    `create_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '事件入库时间',
    PRIMARY KEY (`id`),
    KEY `idx_point_time` (`point_id`, `timestamp_ms`),
    KEY `idx_user_time` (`user_id`, `timestamp_ms`),
    KEY `idx_plate` (`plate_number`),
    KEY `idx_track` (`track_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='通行事件明细流水表';

-- 4. 小时级商业指标预聚合表
CREATE TABLE IF NOT EXISTS `biz_stats_hourly` (
    `id` BIGINT NOT NULL AUTO_INCREMENT COMMENT '自增主键',
    `point_id` BIGINT NOT NULL COMMENT '点位ID',
    `user_id` BIGINT NOT NULL COMMENT '用户ID',
    `stat_date` DATE NOT NULL COMMENT '统计日期(yyyy-MM-dd)',
    `stat_hour` TINYINT NOT NULL COMMENT '统计小时段(0~23)',
    `entry_count` INT NOT NULL DEFAULT 0 COMMENT '小时累计进入量',
    `exit_count` INT NOT NULL DEFAULT 0 COMMENT '小时累计离开量',
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
    `current_occupancy` INT NOT NULL DEFAULT 0 COMMENT '当前防区滞留数',
    `last_heartbeat` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '最新心跳时间',
    PRIMARY KEY (`point_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='边缘设备健康心跳表';

-- 6. 业务模型与感知业务类型配置表 (与点位表 biz_type 强关联，原生定义，无 ALTER TABLE)
CREATE TABLE IF NOT EXISTS `sys_business_type` (
    `id` BIGINT NOT NULL AUTO_INCREMENT COMMENT '业务类型主键ID',
    `biz_code` VARCHAR(32) NOT NULL COMMENT '业务模式英文唯一标识(与边缘端算法协议及点位biz_type一致,如PEDESTRIAN/VEHICLE)',
    `biz_name` VARCHAR(64) NOT NULL COMMENT '业务模式中文名称',
    `description` VARCHAR(500) DEFAULT '' COMMENT '业务定位场景与部署要求描述',
    `features` VARCHAR(1000) DEFAULT '[]' COMMENT '支持的AI算法特征属性(JSON格式标签列表,如性别/年龄/车牌等)',
    `device_req` VARCHAR(500) DEFAULT '' COMMENT '前端采集与硬件算力要求建议(如推荐分辨率/帧率/角度)',
    `icon` VARCHAR(64) DEFAULT 'User' COMMENT '前端UI展示图标',
    `color_tag` VARCHAR(16) DEFAULT '#388bfd' COMMENT '标签色彩十六进制值',
    `status` TINYINT NOT NULL DEFAULT 1 COMMENT '业务启用状态: 1-启用, 0-停用',
    `sort_order` INT NOT NULL DEFAULT 0 COMMENT '显示排序权重(数值越小越靠前)',
    `is_system` TINYINT NOT NULL DEFAULT 0 COMMENT '系统预置标记: 1-系统内置(禁止删除), 0-自定义扩展',
    `create_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    `update_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_biz_code` (`biz_code`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='业务类型配置与AI特征属性表';

-- 预置种子业务类型: 客流分析 与 车流分析
INSERT INTO `sys_business_type` (`id`, `biz_code`, `biz_name`, `description`, `features`, `device_req`, `icon`, `color_tag`, `status`, `sort_order`, `is_system`)
VALUES 
(1, 'PEDESTRIAN', '客流检测与客群分析', '适用于人员通道、大门、闸机等出入口场景，实时进行人体检测跟踪、双向通行计数及性别、年龄段客群画像分析。', '["人体检测","轨迹跟踪","进出防区计数","性别分析","年龄段识别","客流密度"]', '推荐1080P/720P，帧率15-25fps，架高2.5m~3.5m下视角', 'User', '#388bfd', 1, 1, 1),
(2, 'VEHICLE', '车流检测与车辆分析', '适用于停车场出入口、道路卡口、园区车道场景，实时进行机动车检测、车辆出入计数、车牌号码识别及车型、车身颜色分析。', '["车辆检测","车辆跟踪","车牌识别","号牌颜色识别","车型分类","车身颜色分析"]', '推荐1080P/2K，推荐抓拍焦距6~12mm，车道正面角度≤30度', 'Van', '#e3b341', 1, 2, 1)
ON DUPLICATE KEY UPDATE 
    `biz_name`=VALUES(`biz_name`), 
    `description`=VALUES(`description`), 
    `features`=VALUES(`features`), 
    `device_req`=VALUES(`device_req`);
