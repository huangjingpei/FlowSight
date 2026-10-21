---
name: edge-cloud-iot-platform
description: >-
  Architectural blueprint, implementation runbook, and scaffolding guide for Edge-Cloud Collaborative Smart IoT / AI Vision Multi-Tenant Platforms (Edge C++ AI Node + Spring Boot 3 Cloud SaaS + Vue 3 H5/Admin + MinIO Short Video Clip Pipeline). Use when designing, building, or scaffolding edge-cloud collaborative IoT/AI systems, multi-tenant vision analytics, edge-to-cloud sync with local autonomy, or mobile monitoring dashboards.
---

# 边-云-端协同智能物联与视觉多租户平台建设规范 (Edge-Cloud IoT Vision Platform)

本 Skill 沉淀自 FlowSight 边-云-端协同多租户平台的完整工程实践，提炼了一套高可靠、低带宽消耗、零复杂 NAT 穿透依赖的端到端软硬件协同物联网系统构建方法论与代码脚手架指南。

---

## 适用场景与边界

在面对以下需求或技术选型时，激活并参照本规范：
- **终端现场采集**：边缘设备（C++/Python、OpenCV、YOLO/NPU 推理、本地 SQLite、本地视频短切片缓存）。
- **云端业务中台**：Java 17/24 + Spring Boot 3.x + MyBatis-Plus + MySQL 8.0 + Redis + MinIO 对象存储。
- **多端展示应用**：
  - **移动端 (H5/小程序)**：商户/业主决策大屏（今日客流/事件 KPI、时段趋势、客群画像、短切片秒级回放）。
  - **管理运营端 (Web PC)**：租户管理、点位设备纳管、边缘 Token 生成与重置、权限 RBAC。
- **核心业务痛点解决**：现场网络无公网 IP、动态带宽受限、回放点位状态竞争、边缘与云端数据生命周期隔离。

---

## 核心架构原则与设计决议 (ADR)

### 1. 视频回放管线：云端转存方案 (方案 A) vs 局域网穿透 (方案 B)
- **决议**：**一律采用方案 A（MinIO/云端轻量转存 + H5 标准 HTTP/HTTPS 秒开回放）**。
- **技术理由**：
  - 边缘设备常处于商户 NAT4/动态内网或 4G/5G 路由器后，WebRTC/P2P 打洞成功率不足 70%，且需要搭建专用 TURN 中继服务器（维护成本极高）。
  - 业务场景主要为**事件触发短切片**（5~30秒，H.264/MP4，每个文件仅 1~5MB），边缘 AI 检测到事件后单向 HTTPS PUT 异步上传至 MinIO。
  - 手机 H5 仅需通过预签名 GET URL（Presigned URL）经 CDN/对象存储直接秒级拉流播放，完全解耦手机与边缘现场的网络连接。

### 2. 状态机互斥锁定原则 (Mutual Exclusion Lock)
- **决议**：**在切片视频播放期间，移动端 H5 必须硬锁定禁止切换点位**。
- **技术理由**：
  - 避免多媒体解码器、图表重渲染（ECharts）与异步请求之间的竞态条件。
  - 前端状态机设立 `isMediaPlaying` 信号量：当视频弹窗开启或正在播放时，禁用点位切换下拉框，并向用户提示“正在播放切片，请关闭后再切换点位”。

### 3. 边云生命周期与删除隔离原则 (Lifecycle Isolation)
- **决议**：**云端删除点位或数据时，绝对禁止向下级联删除现场边缘设备上的数据**。
- **技术理由**：
  - 边缘客户端具备**完全的本地自治性**（Local Autonomy），本地 SQLite 与 `clips/` 视频池作为现场审计证据，遵循本地循环覆盖策略。
  - 云端删除操作仅物理清理云端资源（MySQL 记录标记或物理清理、MinIO 切片对象移除），手机端无法再查看该点位，但现场工控机/收银机数据完好无损。

### 4. 边缘纳管与安全认证 (Double-Token Isolation)
- **决议**：
  - 租户管理：租户拥有全局唯一 ID（`tenant_id`），支持多点位集中管控。
  - 点位认证：点位设备分配专属 `point_token`。云端数据库**仅存储加盐 SHA-256 散列值**，明文 Token 仅在生成/重置时返回一次。边缘端在 HTTP Header `X-Point-Token` 中携带该 Token，通过过滤器鉴权。

---

## 实施流程与标准脚手架指导

### 阶段一：基础设施编排 (Docker Compose)
必须包含四项基础服务：MySQL 8.0（业务持久化）、Redis 7.0（高频指标缓存与分布式锁）、MinIO（视频切片与事件抓拍 S3 存储）。

详见参考文档：[架构规范与编排](./references/architecture-matrix.md)。

---

### 阶段二：云端 Spring Boot 3 中台搭建规范

#### 1. 技术栈基准与 JDK 24 避坑指南
- **Spring Boot**: `3.2.x` 以上
- **Java 运行时**: 优先 JDK 17 LTS，兼容 JDK 21 / JDK 24。
- **关键避坑**：
  - 若在 JDK 24 下编译，Lombok 必须升级至 **`1.18.38`** 以上，且必须在 `maven-compiler-plugin` 中显式配置 `annotationProcessorPaths`，否则将触发 `java.lang.ExceptionInInitializerError: com.sun.tools.javac.code.TypeTag :: UNKNOWN`。
  - 参考配置：
    ```xml
    <properties>
        <java.version>17</java.version>
        <lombok.version>1.18.38</lombok.version>
    </properties>
    <build>
        <plugins>
            <plugin>
                <groupId>org.apache.maven.plugins</groupId>
                <artifactId>maven-compiler-plugin</artifactId>
                <version>3.13.0</version>
                <configuration>
                    <annotationProcessorPaths>
                        <path>
                            <groupId>org.projectlombok</groupId>
                            <artifactId>lombok</artifactId>
                            <version>${lombok.version}</version>
                        </path>
                    </annotationProcessorPaths>
                </configuration>
            </plugin>
        </plugins>
    </build>
    ```

#### 2. 数据库幂等自动初始化 (`spring.sql.init`)
在微服务或云端部署时，为保证多环境零运维开箱即用，必须配置数据库自动初始化：
```yaml
spring:
  datasource:
    url: jdbc:mysql://localhost:3306/flowsight_cloud?useUnicode=true&characterEncoding=utf-8&serverTimezone=Asia/Shanghai&createDatabaseIfNotExist=true&allowMultiQueries=true
  sql:
    init:
      mode: always
      schema-locations: classpath:schema.sql
      continue-on-error: true
```
> [!IMPORTANT]
> `schema.sql` 必须全部使用 `CREATE TABLE IF NOT EXISTS` 与幂等 `INSERT IGNORE INTO`，确保重复启动不报错。

#### 3. 核心分层与包结构标准
```text
com.flowsight.cloud
├── common              # 全局常量、统一 Result<T> 响应、全局异常处理 (GlobalExceptionHandler)
├── config              # MinioConfig, SecurityConfig, WebMvcConfig, RedisConfig
├── controller
│   ├── AuthController         # 租户注册/手机密码登录/Token刷新
│   ├── AdminUserController    # 管理员对租户用户的启停与管理
│   ├── PointController        # 点位资产管理、Token 生成与重置
│   ├── EdgeIngestController   # 边缘端上报客流/心跳/视频切片登记 (PointToken 鉴权)
│   └── H5DashboardController  # 手机 H5 综合大屏指标与切片拉取 (JWT 鉴权)
├── entity              # PO 实体类 (Tenant, User, Point, MetricFlowHourly, VideoSlice)
├── mapper              # MyBatis-Plus Mappers
└── service             # 业务服务层接口与实现 (含 MinioStorageService 预签名)
```

---

### 阶段三：接口契约与边云协议

详见接口定义参考：[边云标准通信协议契约](./references/api-contracts.md)。

- 边缘上报心跳：`POST /api/v1/edge/heartbeat` (Header: `X-Point-Token`)
- 边缘上报指标：`POST /api/v1/edge/metrics/flow`
- 边缘上报切片：`POST /api/v1/edge/slice/upload` (上传文件并生成云端 MinIO 存储路径)
- H5 概览大屏：`GET /api/v1/h5/dashboard/summary?pointId={id}`
- H5 视频回放：`GET /api/v1/h5/dashboard/slices?pointId={id}` (返回带有时效性的 MinIO Presigned GET URL)

---

### 阶段四：前端套件建设标准

#### 1. 移动端 H5 (Vue 3 + Vant 4 + Pinia + ECharts)
- **组件结构**：
  - `PointSelector`：点位切换组件，必须注入状态机检查 `isMediaPlaying`，在播放时禁用或弹窗阻止。
  - `KpiCards`：今日总客流、进入人次、离开人次、当前滞留人次。
  - `HourlyFlowChart`：当日 24 小时进入/离开折线/柱状图。
  - `DemographicChart`：性别、年龄分布饼图。
  - `VideoSliceModal`：切片事件列表与基于 HTML5 `<video>` 的标准秒开播放器。
- **Pinia 状态管理**：
  ```typescript
  export const useDashboardStore = defineStore('dashboard', {
    state: () => ({
      currentPointId: null as number | null,
      isMediaPlaying: false, // 核心状态机互斥锁
    }),
    actions: {
      setMediaPlaying(status: boolean) {
        this.isMediaPlaying = status;
      },
      switchPoint(newPointId: number) {
        if (this.isMediaPlaying) {
          throw new Error('正在播放切片，请关闭播放窗口后再切换点位！');
        }
        this.currentPointId = newPointId;
      }
    }
  });
  ```

#### 2. 管理后台 (Vue 3 + Element Plus + Pinia)
- **租户/商户视图**：多租户分配、配额管控、数据审计。
- **点位资产拓扑**：查看点位在线状态（依据边缘 30s 心跳）、复制一次性明文 Token、重置 Token、一键跳转到点位关联商户。

---

## 典型故障排查手册 (Troubleshooting)

遇到编译、部署或运行问题时，查阅：[故障排查指南](./references/troubleshooting.md)。

---

## 快速复用工作流检查清单

在全新工程中应用此架构时，按以下检查清单快速推进：
1. [ ] **确定边界**：现场是否有独立工控机/收银机？网络是否为普通宽带？若是，严格选用方案 A。
2. [ ] **生成脚手架**：一键部署 `docker-compose.yml`（MySQL + Redis + MinIO）。
3. [ ] **导入 DDL**：应用标准 `schema.sql`，建立租户、点位、切片元数据表。
4. [ ] **后端启动**：配置 `application.yml`，启动 Spring Boot 3 服务，验证自动建表。
5. [ ] **前端联调**：引入 Vant H5 模版，注入 `isMediaPlaying` 状态锁。
6. [ ] **边缘对接**：边缘 C++ 模块通过 `libcurl` 接入点位心跳与指标推送 API。
