# 27-FlowSight 业务模型可扩展配置中心与管理中台业务类型管理落地规范

## 1. 业务背景与架构设计原则

在 FlowSight 边云协同智能视觉分析平台中，系统已支持两类核心基础感知业务：
1. **客流检测与客群分析 (`PEDESTRIAN`)**：针对人员通道、闸机、大门出入口场景，实时执行人体目标检测、ByteTrack 轨迹跟踪、防区穿越双向计数，并推断预测行人性别、年龄段、客流流速与密度分布。
2. **车流检测与车辆分析 (`VEHICLE`)**：针对停车场出入口、道路卡口、园区车道场景，实时执行机动车检测跟踪、车辆进出通行计数，并通过 LPR OCR 引擎推断车牌号码、号牌颜色、车辆类型与车身颜色。

随着商业化部署的拓展，平台未来将持续引入更多垂直感知业务（例如：安全帽与反光衣合规穿戴巡检、禁区入侵报警、明火烟雾检测、客流滞留超时防作弊等）。

为了支持业务模式的高度可扩展性，平台确立了如下核心协同原则：
> **「先定业务、边端算法就绪、再建点位、动态下发」**
> - **全局唯一标识**：每一个业务模式拥有全局唯一大写英文代号（`biz_code`，如 `PEDESTRIAN`, `VEHICLE`），此代号同时作为点位资产表 `biz_point.biz_type` 的关联键，并作为边缘采集端（FlowSight.exe）热重载算法模型的调度协议。
> - **前置定义与约束**：管理员在为商户创建监控点位之前，如果需要使用新感知业务，必须先在云端中台定义业务模型与分析属性要求，**且前提是现场采集端已经具备相应的视觉模型与流水处理支持**。
> - **强引用安全防护**：当某个业务模式下尚有运行中的监控点位绑定时，系统强制拒绝删除，杜绝因误删业务导致点位协议失联。

---

## 2. 数据库表结构定义与种子数据

数据表在云端 MySQL 数据库原生定义于 [`schema.sql`](file:///e:/FlowSight/solution/backend/src/main/resources/schema.sql)：

```sql
-- 业务模型与感知业务类型配置表 (与点位表 biz_type 强关联)
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

-- 预置种子业务类型
INSERT INTO `sys_business_type` (`id`, `biz_code`, `biz_name`, `description`, `features`, `device_req`, `icon`, `color_tag`, `status`, `sort_order`, `is_system`)
VALUES 
(1, 'PEDESTRIAN', '客流检测与客群分析', '适用于人员通道、大门、闸机等出入口场景，实时进行人体检测跟踪、双向通行计数及性别、年龄段客群画像分析。', '["人体检测","轨迹跟踪","进出防区计数","性别分析","年龄段识别","客流密度"]', '推荐1080P/720P，帧率15-25fps，架高2.5m~3.5m下视角', 'User', '#388bfd', 1, 1, 1),
(2, 'VEHICLE', '车流检测与车辆分析', '适用于停车场出入口、道路卡口、园区车道场景，实时进行机动车检测、车辆出入计数、车牌号码识别及车型、车身颜色分析。', '["车辆检测","车辆跟踪","车牌识别","号牌颜色识别","车型分类","车身颜色分析"]', '推荐1080P/2K，推荐抓拍焦距6~12mm，车道正面角度≤30度', 'Van', '#e3b341', 1, 2, 1)
ON DUPLICATE KEY UPDATE 
    `biz_name`=VALUES(`biz_name`), 
    `description`=VALUES(`description`), 
    `features`=VALUES(`features`), 
    `device_req`=VALUES(`device_req`);
```

---

## 3. 云端中台后端分层架构

- **实体类**：[`BusinessType.java`](file:///e:/FlowSight/solution/backend/src/main/java/com/flowsight/cloud/entity/BusinessType.java)
- **数据访问层**：[`BusinessTypeMapper.java`](file:///e:/FlowSight/solution/backend/src/main/java/com/flowsight/cloud/mapper/BusinessTypeMapper.java)
  - 内置 `countActivePointsByBizType(@Param("bizCode") String bizCode)`，实时统计正在运行绑定的未删除点位数。
- **业务传输对象**：
  - [`BusinessTypeDTO.java`](file:///e:/FlowSight/solution/backend/src/main/java/com/flowsight/cloud/dto/BusinessTypeDTO.java)：录入/编辑入参校验（`bizCode` 格式正则 `^[A-Z0-9_]{2,32}$`）。
  - [`BusinessTypeVO.java`](file:///e:/FlowSight/solution/backend/src/main/java/com/flowsight/cloud/vo/BusinessTypeVO.java)：管理列表出参（带 `pointCount` 绑定统计与 `featureList` 解析列表）。
  - [`BusinessTypeOptionVO.java`](file:///e:/FlowSight/solution/backend/src/main/java/com/flowsight/cloud/vo/BusinessTypeOptionVO.java)：轻量快选模型供点位创建下拉使用。
- **服务层实现**：[`BusinessTypeServiceImpl.java`](file:///e:/FlowSight/solution/backend/src/main/java/com/flowsight/cloud/service/impl/BusinessTypeServiceImpl.java)
  - `pageBusinessTypes`：支持分页与关键字模糊检索。
  - `listActiveOptions`：仅查询 `status = 1` 的启用业务并按排序权重返回。
  - `createBusinessType`：编码自动转大写，排重校验。
  - `updateBusinessType`：禁止篡改系统内置业务的编码，重名防冲突。
  - `deleteBusinessType`：双重校验拦截——系统内置业务禁止删除；有在用点位禁止删除。
  - `toggleStatus`：快捷切换停用与启用。
- **控制层路由**：[`BusinessTypeController.java`](file:///e:/FlowSight/solution/backend/src/main/java/com/flowsight/cloud/controller/BusinessTypeController.java)
  - `GET /api/v1/business-types` (管理员分页查看)
  - `GET /api/v1/business-types/options` (点位下拉快选数据源)
  - `POST /api/v1/business-types` (管理员新增)
  - `PUT /api/v1/business-types/{id}` (管理员修改)
  - `DELETE /api/v1/business-types/{id}` (管理员删除)
  - `PATCH /api/v1/business-types/{id}/status` (启用/停用快速切换)
- **动态唯一点位编码分配**：
  在 [`PointServiceImpl.java`](file:///e:/FlowSight/solution/backend/src/main/java/com/flowsight/cloud/service/impl/PointServiceImpl.java#L538-L553) 中，根据传入的 `bizType` 自动提取前缀（如 `PT_PED_`、`PT_VEH_`、`PT_FIRE_` 等），支持任意业务类型自动化分配。

---

## 4. 管理中台前端设计与联动

- **全新管理视图**：[`BusinessTypeManage.vue`](file:///e:/FlowSight/solution/apps/admin/src/views/BusinessTypeManage.vue)
  - **规范引导条**：提示边端协同要求与模型支持前提。
  - **数据表格**：清晰展示业务模式代号、彩色标签名称、支持的 AI 特征属性列表、采集端算力与角度要求、在用点位数、状态开关、操作按钮。
  - **新增/编辑弹窗**：
    - 业务代号大写自动转化；
    - AI 特征属性支持快捷勾选预设（客流类、车流类、安防类）并支持自定义回车新增标签；
    - 图标选择器与 5 种预设品牌色彩（科技蓝、客流绿、车流金、警示红、智能紫）。
- **点位资产管理视图改造**：[`PointManage.vue`](file:///e:/FlowSight/solution/apps/admin/src/views/PointManage.vue)
  - 顶部过滤选择框由写死二元选项改造为动态拉取 `/api/v1/business-types/options`；
  - 点位列表表格中的“感知业务模式”列动态渲染业务类型名称与对应的品牌颜色；
  - 点位新建与编辑弹窗中，业务模式单选按钮组根据后台业务配置动态生成；切换业务模式时，自动联动展示该业务的场景要求与支持分析属性标签，并自动刷新对应业务前缀的唯一点位编码。
- **导航菜单与路由接入**：
  - 在 [`Layout.vue`](file:///e:/FlowSight/solution/apps/admin/src/views/Layout.vue#L24-L30) 中新增“业务类型管理”一级菜单项；
  - 在 [`router/index.ts`](file:///e:/FlowSight/solution/apps/admin/src/router/index.ts#L18) 中注册 `/business-types` 路由。

---

## 5. 验证与测试结论

1. **MySQL 数据表与种子数据**：`sys_business_type` 表初始化完成，预置 `PEDESTRIAN` 与 `VEHICLE` 完整数据。
2. **后端单元与接口测试**：
   - 登录认证后请求 `/api/v1/business-types/options` 正确返回 2 种业务模式及其属性列表。
   - 请求 `/api/v1/business-types` 正确返回分页列表，并实时统计得出 `PEDESTRIAN` 正在被 1 个点位使用，`VEHICLE` 为 0 个。
   - 新增自定义业务 `FIRE_SAFETY` 成功（ID=3）。
   - 更新业务名称与属性成功。
   - 针对 `PEDESTRIAN` 执行删除，成功触发防护拦截：`系统内置业务类型 [客流检测与客群分析] 属于核心骨干基础模式，禁止直接删除！`。
   - 删除自定义业务成功。
3. **前端构建验证**：运行 `npm run build`，Vite 生产包构建成功（Exit code 0）。
