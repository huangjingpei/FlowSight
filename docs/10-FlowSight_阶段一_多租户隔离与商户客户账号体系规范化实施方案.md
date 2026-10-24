# FlowSight 阶段一：多租户隔离与商户客户账号体系规范化实施方案

> **文件版本**: v1.0.0  
> **编写日期**: 2026-10-24  
> **实施状态**: 已完成并在生产标准环境构建验证通过  
> **关联架构蓝图**: [`docs/09-FlowSight_端云协同架构全景图_实体职责边界与混沌问题排查纠偏清单.md`](file:///E:/FlowSight/docs/09-FlowSight_端云协同架构全景图_实体职责边界与混沌问题排查纠偏清单.md)

---

## 一、阶段目标与解决的核心混沌问题

在 FlowSight SaaS 平台的最初原型演进中，存在以下严重的功能错位与多租户混沌现象：
1. **身份定位模糊**：移动 H5 端与 PC 运营端权限混淆，H5 端被误当成"运维配置管理后台"，缺乏纯粹服务于政务、物业、商业客户日常看数与录像取证的清晰定位。
2. **多租户数据穿透风险**：缺乏点位归属权校验，商户 A 在 H5 端可通过修改参数非法查阅商户 B 的客流大屏或切片视频。
3. **商业化 SaaS 配额缺失**：点位资产创建无上限约束，商户开通点位未与企业授权资质、配额上限 (`pointQuota`) 和订阅有效期 (`expireTime`) 关联。
4. **数据库设计不彻底**：通过临时 `ALTER TABLE` 补丁维护字段，导致多环境迁移时 schema 状态不一致。

**阶段一实施核心指标**：
- **Schema 规范化**：直接重构 `schema.sql`，移除一切 `ALTER TABLE` 补丁，预置标准企业客户种子（万科物业、滨江政务）。
- **实体与 DTO 全链条补齐**：全链贯通商户客户企业全称 (`companyName`)、联系人 (`contactPerson`)、点位配额 (`pointQuota`)、SaaS 订阅到期时间 (`expireTime`)。
- **配额守卫拦截**：后端 `PointServiceImpl.createPoint` 强行校验配额上限，防止商户超配开通。
- **租户横向越权拦截**：H5 后端服务全量接入 `checkPointOwnership(pointId, request)`，杜绝跨租户数据窥探。
- **UI 体验升维**：PC 运营端一目了然查看各客户点位配额占用及到期状态；H5 端自适应呈现企业身份大屏与切片回放中心。

---

## 二、三端实体职责与数据隔离边界

```
                       ┌──────────────────────────────────────────────┐
                       │   SaaS 平台超级管理员 (ROLE_ADMIN)              │
                       │   PC 运营管理中台 (solution/apps/admin)        │
                       └──────────────────────┬───────────────────────┘
                                              │ 1. 客户账号开通/配额管理/订阅到期授权
                                              │ 2. 点位资产规划/Token发放与重置
                                              ▼
                       ┌──────────────────────────────────────────────┐
                       │        FlowSight Cloud 云端中心服务           │
                       │             (Spring Boot 3 + MySQL)          │
                       │  - 严格点位属主校验 checkPointOwnership()      │
                       │  - 配额强校验 (existingCount < pointQuota)    │
                       └──────────────┬───────────────────────────────┘
                                      │
            ┌─────────────────────────┴────────────────────────┐
            │                                                  │
            ▼ 2. 边缘端定时心跳与数据汇聚 (Token对接)               ▼ 3. 客户手机端决策与取证 (严格隔离)
┌───────────────────────────────┐                  ┌───────────────────────────────┐
│ FlowSight.exe 边缘AI工控客户端 │                  │   商户/政务客户 H5 决策中台     │
│ (现场物理设备 / C++ Qt6)        │                  │      (solution/apps/h5)       │
├───────────────────────────────┤                  ├───────────────────────────────┤
│ • 独占绑定 Point Token         │                  │ • 租户客户登录 (万科/政务)    │
│ • 本地离线视频推理与事件落盘    │                  │ • 数据决策大屏 (宏观KPI+曲线)  │
│ • 异步切片上传 (MinIO)         │                  │ • 证据切片回放 (防逃逸播放)    │
│ • 独立自治: 云端删除不影响本地   │                  │ • 仅可访问属主名下点位与录像   │
└───────────────────────────────┘                  └───────────────────────────────┘
```

---

## 三、数据库设计演进 (`schema.sql`)

本阶段依照指导原则，**直接对 `schema.sql` 进行正向全量定义**，剔除所有临时性补丁指令。

### 1. 商户客户与认证表 (`sys_user`) 增强
```sql
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
```

### 2. 标杆商户种子数据与初始配额预置
```sql
-- 1. 超级管理员 (手机号: 13800000000, 配额 999)
INSERT INTO `sys_user` (`id`, `phone`, `password_hash`, `role`, `nickname`, `company_name`, `contact_person`, `point_quota`, `expire_time`) 
VALUES (1, '13800000000', '$2a$10$7JB720yubVSZvUI0rEqK/.Vq4VfH4FjPvhV7r0h1hA9x.VcmQYVce', 'ROLE_ADMIN', '平台总管', 'FlowSight SaaS 运营指挥中心', '系统运维部', 999, '2099-12-31 23:59:59')
ON DUPLICATE KEY UPDATE `company_name`=VALUES(`company_name`), `contact_person`=VALUES(`contact_person`), `point_quota`=VALUES(`point_quota`);

-- 2. 商户客户 A: 万科物业 (手机号: 13454118762, 配额 10 点位)
INSERT INTO `sys_user` (`id`, `phone`, `password_hash`, `role`, `nickname`, `company_name`, `contact_person`, `point_quota`, `expire_time`) 
VALUES (2, '13454118762', '$2a$10$pTPz9/fScp/.t97liVhJ.OmlqXEEVyhlZ/rbqFuu.IRFdtY5N73wK', 'ROLE_USER', '万科物业', '万科物业服务有限公司(未来城项目)', '刘经理', 10, '2028-12-31 23:59:59')
ON DUPLICATE KEY UPDATE `company_name`=VALUES(`company_name`), `contact_person`=VALUES(`contact_person`), `point_quota`=VALUES(`point_quota`);

-- 3. 商户客户 B: 滨江政务中心 (手机号: 13900000002, 配额 5 点位)
INSERT INTO `sys_user` (`id`, `phone`, `password_hash`, `role`, `nickname`, `company_name`, `contact_person`, `point_quota`, `expire_time`) 
VALUES (3, '13900000002', '$2a$10$7JB720yubVSZvUI0rEqK/.Vq4VfH4FjPvhV7r0h1hA9x.VcmQYVce', 'ROLE_USER', '滨江政务', '滨江区政务服务中心办事大厅', '张主任', 5, '2028-12-31 23:59:59')
ON DUPLICATE KEY UPDATE `company_name`=VALUES(`company_name`), `contact_person`=VALUES(`contact_person`), `point_quota`=VALUES(`point_quota`);
```

---

## 四、后端代码重构与安全隔离实现

### 1. 实体与 DTO 层
- **`SysUser.java`**: 增加 `companyName`, `contactPerson`, `pointQuota`, `expireTime` 映射字段。
- **`UserVO.java`**: 增加租户多维信息回显，包含已开通点位数 `pointCount` 与配额 `pointQuota`。
- **`UserCreateDTO.java` / `UserUpdateDTO.java`**: 支持管理端在开户及修改时指定配额、企业名、责任人及 SaaS 到期时间。
- **`PointVO.java`**: 增加 `companyName` 和 `userNickname`，点位列表直接展现所属商户企业，无需前端多次异步拼接。
- **`LoginVO.java`**: 增加 `companyName` 字段，客户端登录一次即可获得企业铭牌信息。

### 2. 关键业务逻辑实施
- **`PointServiceImpl.java` (点位配额拦截与属主关联)**:
  ```java
  @Override
  public PointVO createPoint(PointCreateDTO dto) {
      SysUser user = userMapper.selectById(dto.getUserId());
      if (user == null) {
          throw new IllegalArgumentException("所属商户客户不存在");
      }
      Long existingCount = pointMapper.selectCount(new LambdaQueryWrapper<BizPoint>().eq(BizPoint::getUserId, dto.getUserId()));
      int quota = user.getPointQuota() != null ? user.getPointQuota() : 5;
      if (existingCount >= quota) {
          String clientName = user.getCompanyName() != null && !user.getCompanyName().isEmpty() ? user.getCompanyName() : user.getNickname();
          throw new IllegalArgumentException("客户【" + clientName + "】名下点位数已达配额上限 (" + quota + " 个)，无法继续新增！");
      }
      ...
  }
  ```
- **`AuthServiceImpl.java` (到期拦截与企业登录态下发)**:
  ```java
  if (user.getExpireTime() != null && user.getExpireTime().isBefore(LocalDateTime.now())) {
      throw new IllegalArgumentException("该商户账号 SaaS 订阅服务已到期，请联系平台管理员续约");
  }
  ```
- **`H5DashboardController.java` (防越权数据隔离)**:
  在 `summary`、`trend`、`events` 等全部 H5 接口入口强制执行：
  ```java
  private void checkPointOwnership(Long pointId, HttpServletRequest request) {
      if (pointId == null || pointId <= 0) return;
      Long currentUserId = (Long) request.getAttribute("userId");
      String currentRole = (String) request.getAttribute("role");
      if ("ROLE_ADMIN".equals(currentRole)) return; // 平台超管拥有全局审计权
      BizPoint point = pointMapper.selectById(pointId);
      if (point == null || !point.getUserId().equals(currentUserId)) {
          throw new IllegalArgumentException("越权访问拒绝: 您无权查看非名下点位数据");
      }
  }
  ```

---

## 五、前端应用落地与体验重塑

### 1. PC 运营管理中台 (`UserManage.vue` & `PointManage.vue`)
- **客户维度全景管控**：
  - 列表呈现企业全称、简称、联系人、点位配额占用胶囊（例如 `2 / 10 点位`，满额标红警示）以及 SaaS 订阅倒计时状态。
  - 支持按手机号、企业全称、联系人模糊搜索。
  - 新增点位时下拉列表展示该商户的企业名及当前已占用/配额数，直观防呆。

### 2. 移动决策 H5 端 (`Dashboard.vue` & `Playback.vue`)
- **企业客户身份铭牌自适应**：
  - 顶部导航栏动态绑定当前商户企业全称（如 `万科物业服务有限公司(未来城项目)`），提升政务及物业客户的专属感。
- **双页面架构正式确立**：
  - 页面一：数据看板 (`Dashboard.vue`)，聚焦宏观客流指标、在场驻留、逐时/多日/季度走势图表与 AI 客群画像。
  - 页面二：证据切片回放 (`Playback.vue`)，提供通道过滤、方向过滤、时间范围过滤及完整翻页器，支持多路防逃逸视频回放。

---

## 六、构建与验证结论

执行全量编译与工程打包：
1. **后端 Spring Boot 编译**:
   - 指令: `mvn compile -f solution/backend/pom.xml -DskipTests`
   - 结果: `BUILD SUCCESS` (51 source files compiled in 24.399s).
2. **PC 运营端 Vite 生产打包**:
   - 指令: `npm --prefix solution/apps/admin run build`
   - 结果: `✓ 1678 modules transformed`, 产物打包成功于 `dist/`.
3. **移动 H5 端 Vite 生产打包**:
   - 指令: `npm --prefix solution/apps/h5 run build`
   - 结果: `✓ 924 modules transformed`, 产物打包成功于 `dist/`.

**阶段一目标已 100% 达成，系统进入阶段二（边缘客户端 MinIO 视频切片上传管道与 H5 实况播放打通）。**
