# 常见故障排查手册 (Troubleshooting Guide)

本文件汇总在搭建与运行边-云-端协同多租户物联网平台时最容易踩坑的典型问题与标准解决方案。

---

## 1. JDK 24 编译报错：`TypeTag :: UNKNOWN`

### 现象
在使用了 Lombok 的 Spring Boot 3 工程中，若开发或构建环境采用了 JDK 24，在执行 `javac` 或 Maven 编译时报错：
```text
java: java.lang.ExceptionInInitializerError
com.sun.tools.javac.code.TypeTag :: UNKNOWN
Errors occurred while compiling module 'flowsight-cloud-backend'
javac 24.0.1 was used to compile java sources
```

### 根本原因
JDK 24 对内部编译器语法树节点枚举（`TypeTag`）进行了重构，旧版 Lombok（1.18.30 ~ 1.18.36）尚未适配 JDK 24 内部 API 变更，导致注解处理器初始化失败。

### 标准解决方案
1. 将 `pom.xml` 中的 Lombok 升级至 **`1.18.38`** 或更高版本：
   ```xml
   <properties>
       <lombok.version>1.18.38</lombok.version>
   </properties>
   ```
2. 确保在 `maven-compiler-plugin` 中显式指定 `annotationProcessorPaths`：
   ```xml
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
   ```

---

## 2. Spring Boot 数据库自动初始化 `schema.sql` 报错

### 现象
配置了 `spring.sql.init.mode=always` 后，启动服务时报 `Communications link failure` 或 SQL 语法错误、表已存在错误。

### 关键配置项自查
1. **JDBC 连接串必须开启多语句与自动建库**：
   ```properties
   spring.datasource.url=jdbc:mysql://localhost:3306/flowsight_cloud?useUnicode=true&characterEncoding=utf-8&serverTimezone=Asia/Shanghai&createDatabaseIfNotExist=true&allowMultiQueries=true
   ```
   - `createDatabaseIfNotExist=true`：避免数据库未手工创建时抛出找不到 Database 异常；
   - `allowMultiQueries=true`：允许执行包含分号的多条 DDL/DML 脚本。
2. **DDL 必须严格具备幂等性**：
   - 建表语句必须为 `CREATE TABLE IF NOT EXISTS`；
   - 初始数据插入必须使用 `INSERT IGNORE INTO` 或 `INSERT INTO ... ON DUPLICATE KEY UPDATE`，避免第二次启动因主键冲突挂起。
3. **初始化容错配置**：
   ```yaml
   spring:
     sql:
       init:
         mode: always
         continue-on-error: true
   ```

---

## 3. MinIO 预签名播放 URL 在外网或手机端无法访问

### 现象
云端成功生成了预签名 URL（如 `http://127.0.0.1:9000/bucket/clip.mp4?...`），但在手机端或公网环境下，因主机名是内网 IP 或 localhost 导致视频加载失败。

### 标准解决方案
在配置 MinIO 时区分 **内部通信地址 (Endpoint)** 与 **外网访问地址 (External Endpoint)**：
```yaml
minio:
  endpoint: http://minio:9000       # 云端服务与 MinIO 容器内部通信
  external-endpoint: http://api.yourdomain.com:9000 # 手机端/前端公网访问基准
```
在生成 Presigned URL 时，将内部主机名前缀替换为 `external-endpoint`，或者通过 Nginx 反向代理将 `/minio/` 路径透明转发到内网 MinIO 服务。

---

## 4. 移动端 H5 点位切换与切片播放竞态死锁

### 现象
商户在点开切片播放视频的同时快速在下拉菜单中切换点位，导致原点位的视频解码器未销毁，新点位的图表请求又并发返回，界面数据显示混乱甚至页面白屏。

### 标准解决方案
在 Pinia 状态管理层增加 **硬锁定** 逻辑：
1. 声明 `isMediaPlaying: boolean`。
2. 当打开切片视频弹窗或 `<video>` 触发 `onplay` 时，设置 `isMediaPlaying = true`。
3. 在点位下拉框的 `@change` 或 `before-change` 钩子中判断：
   ```javascript
   if (dashboardStore.isMediaPlaying) {
     showToast('正在播放切片，请关闭后再切换点位');
     return false; // 拦截切换
   }
   ```
4. 当弹窗关闭（`close`）或视频播放完毕时，重置 `isMediaPlaying = false`。
