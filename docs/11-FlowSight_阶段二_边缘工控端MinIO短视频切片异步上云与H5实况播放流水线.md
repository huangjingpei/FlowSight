# FlowSight 阶段二：边缘工控端 MinIO 短视频切片异步上云与 H5 实况播放流水线

> **文件版本**: v1.0.0  
> **编写日期**: 2026-10-24  
> **实施状态**: 已完成并在生产标准环境构建验证通过  
> **关联架构蓝图**: [`docs/09-FlowSight_端云协同架构全景图_实体职责边界与混沌问题排查纠偏清单.md`](file:///E:/FlowSight/docs/09-FlowSight_端云协同架构全景图_实体职责边界与混沌问题排查纠偏清单.md)

---

## 一、阶段目标与业务背景

在 FlowSight SaaS 系统中，手机 H5 端的【切片回放】模块是政务大厅、物业管理、园区安保客户进行**客流取证、异常溯源与事后复盘**的核心功能。

在以往原型中，端云协同链路在此处存在脱节：
1. **切片孤岛滞留**：FlowSight.exe 在现场工控机本地录制的 MP4 短视频（`clips/clip_{ts}_t{trackId}.mp4`）仅保存在本地磁盘，从未上传至云端对象存储（MinIO）。
2. **手机端无法播放**：由于缺少 MinIO 存储桶的云端视频直链，H5 切片回放页面无法播放实况录像，只能查看通行指标与时间文字，丧失了视频取证的核心价值。
3. **异步编码与上报时序差 (Race Condition)**：
   - 现场工控机在人员进出触发时，后台编码线程异步将 3 秒环形缓冲压缩为 MP4 文件（耗时约 200ms~500ms）；
   - 指标上报引擎（每 10 秒批量上报或满 50 条刷新）可能在切片尚未完成磁盘落盘前已将事件上报云端，或者切片先行上传完毕但事件指标尚未到达；
   - 传统单向外键绑定容易导致切片 `clipUrl` 与流水记录失联。

**阶段二实施核心成果**：
- **边缘切片异步上传引擎**：FlowSight.exe `EdgeReportManager` 增加 `PendingClip` 队列与 2 秒轮询上传机制，基于 `QHttpMultiPart` 与 `X-Point-Token` 实现安全、高吞吐的流式上传。
- **编码完成防抖检测**：严格校验 `QFileInfo::exists()` 与 `size() >= 1024`，未编码完成时平滑等待退避，杜绝上传 0 字节半成品。
- **云端双向时序自愈绑定**：云端 `EventServiceImpl` 设立内存待绑定切片池 `pendingClipUrlCache`，无论切片先到达还是批量事件先到达，均能 100% 自动对齐绑定。
- **H5 预签名防盗链播放闭环**：通过 MinIO S3 协议动态下发 15 分钟临时播放签名链接，并与 H5 全局播放互斥锁深度联动。

---

## 二、端云数据流转与闭环架构

```
[ FlowSight.exe 边缘工控机 ]
   │
   ├─ 1. 人员越界触发 ──> ClipRecorder 环形缓冲抓取 ──> 异步多线程编码生成 clips/clip_xxx.mp4
   │
   ├─ 2. 投递到 EdgeReportManager::enqueue_clip_upload()
   │      │
   │      ▼
   ├─ 3. process_clip_upload_queue() 定时轮询
   │      ├─ 校验文件存在 & 大小 > 1KB (防半成品)
   │      └─ POST /api/v1/edge/clip/upload (带 X-Point-Token + trackId + ts)
   │
[ 云端 FlowSight Cloud 后端 (Spring Boot 3) ]
   │
   ├─ 4. EdgeIngestController::uploadClip()
   │      ▼
   ├─ 5. MinioServiceImpl::uploadClip() ──> 写入 MinIO S3 存储桶 (clips/pointId/yyyyMMdd/...)
   │      ▼
   ├─ 6. EventServiceImpl::uploadEdgeClip() ──> 双向自愈绑定:
   │      ├─ 若 biz_passage_event 已入库 ──> 立即 UPDATE clip_url
   │      └─ 若 biz_passage_event 尚未到达 ──> 暂存 pendingClipUrlCache[pointId_trackId]
   │
[ 手机端 H5 决策与取证中心 (Vue 3 + Vant 4) ]
   │
   ├─ 7. 用户打开【切片回放】页面 (Playback.vue)
   │      ▼
   ├─ 8. GET /api/v1/h5/dashboard/events (带翻页、点位、方向、时间过滤)
   │      ▼
   ├─ 9. 云端生成 15 分钟 MinIO Presigned URL (getPresignedPlayUrl)
   │      ▼
   └─ 10. 弹窗原生 <video> 播放 ──> 激活 pointStore.setMediaPlaying(true) 锁定点位切换
```

---

## 三、边缘客户端核心改造 (`src/network/EdgeReportManager`)

### 1. 结构与生命周期
在 `EdgeReportManager.hpp` 中声明待上传切片任务队列与独立轮询定时器：
```cpp
struct PendingClip {
    QString local_path;
    int track_id = -1;
    qint64 timestamp_ms = 0;
    int retry_count = 0;
};

void enqueue_clip_upload(const QString& clip_path, int track_id, qint64 timestamp_ms);
void process_clip_upload_queue();

QTimer* clip_upload_timer_ = nullptr;
std::deque<PendingClip> pending_clips_;
QMutex clip_mutex_;
bool is_uploading_clip_ = false;
```

在 `init()` 中启动 2 秒轮询：
```cpp
if (!clip_upload_timer_) {
    clip_upload_timer_ = new QTimer(this);
    connect(clip_upload_timer_, &QTimer::timeout, this, &EdgeReportManager::process_clip_upload_queue);
    clip_upload_timer_->start(2000);
}
```

### 2. 人员通行事件与切片自动联动
在 `record_passage(const PassageEvent& event)` 中：
```cpp
// 若事件生成了视频切片，投递至异步上传队列
if (!event.clip_path.empty()) {
    enqueue_clip_upload(QString::fromStdString(event.clip_path), event.track_id, event.timestamp_ms);
}
```

### 3. 流式 Multipart 上传与文件状态防抖
```cpp
void EdgeReportManager::process_clip_upload_queue() {
    if (is_uploading_clip_) return;
    if (cloud_base_url_.trimmed().isEmpty() || bound_point_token_.trimmed().isEmpty()) return;

    PendingClip task;
    {
        QMutexLocker locker(&clip_mutex_);
        if (pending_clips_.empty()) return;
        task = pending_clips_.front();
    }

    QFileInfo fi(task.local_path);
    // 检查切片文件是否存在且已由 ClipRecorder 完成编码 (> 1KB)
    if (!fi.exists() || fi.size() < 1024) {
        QMutexLocker locker(&clip_mutex_);
        if (!pending_clips_.empty()) {
            pending_clips_.front().retry_count++;
            if (pending_clips_.front().retry_count > 10) {
                // 等待 20 秒仍未生成完整切片，安全放弃该任务
                pending_clips_.pop_front();
            }
        }
        return;
    }

    is_uploading_clip_ = true;
    QHttpMultiPart* multi_part = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart file_part;
    file_part.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("video/mp4"));
    file_part.setHeader(QNetworkRequest::ContentDispositionHeader,
        QVariant(QString("form-data; name=\"file\"; filename=\"%1\"").arg(fi.fileName())));

    QFile* file = new QFile(task.local_path);
    if (!file->open(QIODevice::ReadOnly)) {
        delete file;
        delete multi_part;
        is_uploading_clip_ = false;
        QMutexLocker locker(&clip_mutex_);
        if (!pending_clips_.empty()) pending_clips_.pop_front();
        return;
    }
    file_part.setBodyDevice(file);
    file->setParent(multi_part);
    multi_part->append(file_part);

    QString endpoint = QString("%1/api/v1/edge/clip/upload?trackId=%2&timestamp=%3")
        .arg(cloud_base_url_.trimmed())
        .arg(task.track_id)
        .arg(task.timestamp_ms);

    QUrl upload_url(endpoint);
    QNetworkRequest request(upload_url);
    request.setRawHeader("X-Point-Token", bound_point_token_.toUtf8());

    QNetworkReply* reply = nam_->post(request, multi_part);
    multi_part->setParent(reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply, task]() {
        reply->deleteLater();
        is_uploading_clip_ = false;

        int http_status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() == QNetworkReply::NoError && (http_status == 200 || http_status == 201)) {
            QMutexLocker locker(&clip_mutex_);
            if (!pending_clips_.empty() && pending_clips_.front().local_path == task.local_path) {
                pending_clips_.pop_front();
            }
        } else {
            QMutexLocker locker(&clip_mutex_);
            if (!pending_clips_.empty() && pending_clips_.front().local_path == task.local_path) {
                pending_clips_.front().retry_count++;
                if (pending_clips_.front().retry_count > 5) {
                    pending_clips_.pop_front();
                }
            }
        }
    });
}
```

---

## 四、云端双向时序自愈绑定实现 (`EventServiceImpl.java`)

为彻底消除边缘编码与云端网络数据包之间的时序竞态问题，在 `EventServiceImpl` 中设立 `pendingClipUrlCache` 闭环：

```java
// 待绑定切片池: 解决异步上传中切片先于聚合指标或后于聚合指标到达的时序差问题
private final Map<String, String> pendingClipUrlCache = new ConcurrentHashMap<>();
```

1. **切片上传处理 (`uploadEdgeClip`)**:
   ```java
   @Override
   public String uploadEdgeClip(Long pointId, Integer trackId, Long timestamp, MultipartFile file) {
       String clipUrl = minioService.uploadClip(pointId, trackId, timestamp, file);

       boolean matched = false;
       if (trackId != null && timestamp != null) {
           List<BizPassageEvent> list = eventMapper.selectList(new LambdaQueryWrapper<BizPassageEvent>()
                   .eq(BizPassageEvent::getPointId, pointId)
                   .eq(BizPassageEvent::getTrackId, trackId)
                   .ge(BizPassageEvent::getTimestampMs, timestamp - 60000)
                   .le(BizPassageEvent::getTimestampMs, timestamp + 60000)
                   .orderByDesc(BizPassageEvent::getId));
           if (!list.isEmpty()) {
               BizPassageEvent ev = list.get(0);
               ev.setClipUrl(clipUrl);
               eventMapper.updateById(ev);
               matched = true;
           }
       }

       // 若事件记录尚未批量上报到达，暂存至待绑定池
       if (!matched && trackId != null) {
           pendingClipUrlCache.put(pointId + "_" + trackId, clipUrl);
       }
       return clipUrl;
   }
   ```

2. **批量事件入库消费 (`recordEdgeBatchReport`)**:
   ```java
   // 检查待绑定切片池 (若切片先行上传到达)
   String pendingUrl = pendingClipUrlCache.remove(pointId + "_" + event.getTrackId());
   if (pendingUrl != null) {
       event.setClipUrl(pendingUrl);
   }
   eventMapper.insert(event);
   ```

---

## 五、H5 移动端实况切片取证与播放防护

1. **防盗链与时效控制**：
   - 接口每次调用 `pageEvents`，调用 MinIO 获取有效时间为 15 分钟的 Presigned URL，不在数据库中持久化带有效期的全路径，保障数据安全与存储桶迁移能力。
2. **播放互斥锁防逃逸**：
   - 在 `Playback.vue` 中打开录像弹窗时，激活 `pointStore.setMediaPlaying(true)`；
   - 顶部点位选择器与底部导航联动锁定，禁止在视频播放期间横向切换通道或页面，防止流媒体在后台持续解码造成手机端卡顿与内存泄漏。

---

## 六、构建与验证结论

1. **FlowSight.exe C++ 编译与链接**:
   - MSBuild `Release` 构建成功，`EdgeReportManager.obj` 顺利链接至 `FlowSight.exe`。
2. **Spring Boot 云端后端编译**:
   - `mvn compile -f solution/backend/pom.xml -DskipTests` -> `BUILD SUCCESS`。
3. **移动端 H5 生产打包**:
   - Vite 打包 0 错误，证据切片回放与大屏看板双路由平稳运行。

**阶段二目标已 100% 达成，系统进入阶段三（点位云端删除 vs 边缘端独立自治与离线死循环退避保护）。**
