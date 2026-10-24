# FlowSight 阶段三：点位云端销毁与边端独立自治容灾与死循环退避机制

> **文件版本**: v1.0.0  
> **编写日期**: 2026-10-24  
> **实施状态**: 已完成并在生产标准环境构建验证通过  
> **关联架构蓝图**: [`docs/09-FlowSight_端云协同架构全景图_实体职责边界与混沌问题排查纠偏清单.md`](file:///E:/FlowSight/docs/09-FlowSight_端云协同架构全景图_实体职责边界与混沌问题排查纠偏清单.md)

---

## 一、阶段目标与业务背景

在 B2B 边云协同物联网与 SaaS 平台中，**端云生命周期解耦与容灾边界**是最容易产生设计混沌的领域：
1. **点位注销后的端侧死循环风暴（Deadlock Bug）**：
   - 过去，当 SaaS 运营人员在管理后台删除已废弃点位或重置了 Token 时，现场 FlowSight.exe 客户端并不知道。
   - 客户端继续周期性批量上报数据或排空离线队列，云端网关鉴权失败并返回 `HTTP 400 (未授权: 无效的 Point Token)`。
   - 旧版逻辑将一切失败统统归为"网络异常"，并死板地将数据重新入库到本地 SQLite 离线队列表（`biz_offline_report`），同时让 Watchdog 守护线程每 15 秒再次重试！
   - 结果导致离线队列无限堆积、反复发起无效请求、网络与 CPU 资源严重浪费，形成**死循环退避失效**。
2. **点位删除的职责混淆**：
   - 云端删除点位时，**绝对不能下发指令删除或格式化边缘工控机本地的 SQLite 数据库与 clips/ 录像磁盘目录**！
   - 边缘端拥有现场业务的**最高自治权**：即使云端点位注销或完全断网，工控机必须照常连接 RTSP 视频流、进行 AI 跟踪检测、本地画框、落盘本地事件流水，并给现场值班人员清晰展示"点位已下架(本地自治)"状态。

**阶段三实施核心成果**：
- **HTTP 400/401/403 致命鉴权错误拦截与隔离**：严格区分"临时网络中断/5xx故障"与"点位已被云端下架/Token作废"两大异常类别。
- **离线队列毒丸任务破死锁机制**：对被云端判定为无效 Token 的报文，直接从 SQLite 离线队列中剔除，打破无限死循环。
- **边端自主降级与本地自治模式**：激活 `is_point_deactivated_` 状态保护，阻断向云端发起无效请求，重置内存池，本地录像、AI 推理与 SQLite 流水 100% 独立持久化。
- **状态栏与铭牌高可视度自愈**：主界面状态栏即时点亮专属琥珀色胶囊 `⚠️ 点位已下架(本地自治)`，一旦重新扫码或绑定新 Token，自动平滑复位。

---

## 二、端云协同生命周期对比与容灾矩阵

| 场景事件 | 云端 SaaS 处理策略 | 边缘工控客户端 (FlowSight.exe) 处理策略 | 本地数据安全性 |
| :--- | :--- | :--- | :--- |
| **云端点位删除** | 级联删除 MySQL 点位记录、事件明细、心跳与 MinIO 切片池，手机 H5 彻底无法查看 | 接收 HTTP 400 拦截，进入本地自治模式，停止网络轮询，状态栏亮起 `⚠️ 点位已下架(本地自治)` | **100% 保留**：本地 SQLite、配置、`clips/` 磁盘录像毫发无损 |
| **云端重置 Token** | 原 Token 立即作废，生成新 Token `ptk_xxx` | 原 Token 请求收到 400 后主动挂起；用户在客户端设置界面更新 Token 后自愈复位 | **100% 保留** |
| **网络暂时中断 (断网)** | 心跳停止，标记流状态为 `OFFLINE` | 正常推理落盘，指标与事件自动转存 SQLite 离线队列，Watchdog 周期探活 | **100% 保留** |
| **网络恢复 (自愈)** | 接收离线队列补录数据，批量更新聚合表 | Watchdog 自动分批（每次 3 条）排空离线队列，补传成功后删除队列记录 | **零丢失平滑补齐** |

---

## 三、核心源码实现

### 1. 边缘管理器状态声明 (`src/network/EdgeReportManager.hpp`)
```cpp
// 当前是否在线与点位有效性状态
bool is_cloud_online() const { return is_online_; }
bool is_point_deactivated() const { return is_point_deactivated_; }

bool is_online_ = false;
bool is_point_deactivated_ = false;
bool is_draining_ = false;
```

### 2. 批量指标上报死循环防御 (`EdgeReportManager.cpp::flush_batch`)
```cpp
void EdgeReportManager::flush_batch() {
    QMutexLocker locker(&mutex_);
    if (batch_events_.empty() && in_increment_ == 0 && out_increment_ == 0) return;

    qint64 now_ts = QDateTime::currentMSecsSinceEpoch();

    if (is_point_deactivated_) {
        // 点位在云端已注销或 Token 失效，停止发送网络请求，重置内存池并保持本地自治
        in_increment_ = 0; out_increment_ = 0;
        male_count_ = 0; female_count_ = 0;
        child_count_ = 0; adult_count_ = 0; senior_count_ = 0;
        batch_events_.clear();
        window_start_ts_ = now_ts;
        return;
    }

    ...
    post_compressed_json(upload_url, bound_point_token_, compressed_blob, batch_uuid,
        [this, batch_uuid, compressed_blob, total_events](bool success, int code, const QString& err) {
            if (success) {
                is_online_ = true;
                is_point_deactivated_ = false;
                emit cloud_status_changed(true, "云端同步正常 (HTTP 200)");
                emit batch_reported(true, total_events, 0);
            } else if (code == 400 || code == 401 || code == 403) {
                // 点位在云端已删除、注销或 Token 已被管理员重置/作废！
                // 关键容灾机制: 严禁将死任务存入离线队列表，防止死循环无限重试！
                is_online_ = false;
                is_point_deactivated_ = true;
                qWarning().noquote() << QString("[EdgeReport] Point deactivated or token invalid (HTTP %1: %2). Entering local autonomous mode.")
                                            .arg(code).arg(err);
                emit cloud_status_changed(false, "⚠️ 点位已下架或Token已注销 (本地持续自主监控)");
            } else {
                // 上传失败: 网络抖动/断网/5xx，保存到本地 SQLite 离线队列
                qWarning().noquote() << QString("[EdgeReport] Upload failed (%1, code %2). Saving to SQLite offline queue.")
                                            .arg(err).arg(code);
                DatabaseManager::instance().enqueue_offline_report(batch_uuid, "METRICS_BATCH", compressed_blob);
                is_online_ = false;
                emit cloud_status_changed(false, QString("网络中断，已转存本地离线队列 (%1)").arg(err));
                emit offline_queue_updated(DatabaseManager::instance().get_pending_offline_count());
            }
        });
}
```

### 3. 离线队列表排空毒丸破除 (`EdgeReportManager.cpp::drain_offline_queue`)
```cpp
void EdgeReportManager::drain_offline_queue() {
    if (is_point_deactivated_) return;
    if (cloud_base_url_.trimmed().isEmpty() || bound_point_token_.trimmed().isEmpty()) return;

    auto items = DatabaseManager::instance().fetch_pending_offline_reports(3);
    if (items.empty()) return;

    is_draining_ = true;
    auto item = items.front();

    QUrl upload_url(cloud_base_url_.trimmed() + "/api/v1/edge/metrics/batch");
    post_compressed_json(upload_url, bound_point_token_, item.payload_blob, item.batch_uuid,
        [this, item](bool success, int code, const QString& err) {
            is_draining_ = false;
            if (success) {
                DatabaseManager::instance().remove_offline_report(item.queue_id);
                emit offline_queue_updated(DatabaseManager::instance().get_pending_offline_count());
                drain_offline_queue();
            } else if (code == 400 || code == 401 || code == 403) {
                // 点位在云端已被删除或 Token 失效：直接剔除历史不可达死任务，彻底打破死循环！
                is_point_deactivated_ = true;
                is_online_ = false;
                DatabaseManager::instance().remove_offline_report(item.queue_id);
                qWarning().noquote() << QString("[EdgeReport] Dropped unroutable offline queue #%1 due to point deactivation (HTTP %2)")
                                            .arg(item.queue_id).arg(code);
                emit offline_queue_updated(DatabaseManager::instance().get_pending_offline_count());
                emit cloud_status_changed(false, "⚠️ 点位已下架或Token已注销 (本地持续自主监控)");
            } else {
                DatabaseManager::instance().increment_offline_retry(item.queue_id);
            }
        });
}
```

### 4. 状态栏专属本地自治胶囊呈现 (`src/ui/MainWindow.cpp`)
```cpp
connect(&EdgeReportManager::instance(), &EdgeReportManager::cloud_status_changed, this, [status_cloud](bool is_online, const QString& text) {
    if (text.contains("已注销") || text.contains("已下架") || text.contains("无效")) {
        status_cloud->setText("⚠️ 点位已下架(本地自治)");
        status_cloud->setStyleSheet("background-color: #3b2300; color: #e3b341; border: 1px solid #d29922; border-radius: 4px; padding: 2px 8px; margin: 0 4px; font-weight: 600;");
    } else {
        status_cloud->setText(is_online ? "☁️ 云端在线" : "☁️ 云端离线");
        status_cloud->setStyleSheet(QString(
            "background-color: #21262d; color: %1; border-radius: 4px; padding: 2px 8px; margin: 0 4px; font-weight: 500;"
        ).arg(is_online ? "#3fb950" : "#f85149"));
    }
    status_cloud->setToolTip(text);
});
```

---

## 四、构建与验证结论

1. **FlowSight.exe 构建与链接验证**:
   - 包含新防御逻辑的 `EdgeReportManager.cpp` 和 `MainWindow.cpp` 编译无 warning，链接生成 `FlowSight.exe`。
2. **死锁消除逻辑验证**:
   - 模拟发送无效 Token（HTTP 400）：
     - 客户端控制台打出 `Point deactivated or token invalid (HTTP 400). Entering local autonomous mode.`；
     - 离线队列表未发生死任务累加；
     - 状态栏立即呈现 `⚠️ 点位已下架(本地自治)`；
     - 本地视频流正常推理播放，事件正常插入本地 SQLite。

**阶段三目标已 100% 达成，系统进入阶段四（云端配置动态拉取 /api/v1/edge/config 与边缘自适应热重载）。**
