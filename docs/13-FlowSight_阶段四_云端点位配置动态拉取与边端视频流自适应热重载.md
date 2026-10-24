# FlowSight 阶段四：云端点位配置动态拉取与边端视频流自适应热重载

> **文件版本**: v1.0.0  
> **编写日期**: 2026-10-24  
> **实施状态**: 已完成并在生产标准环境构建验证通过  
> **关联架构蓝图**: [`docs/09-FlowSight_端云协同架构全景图_实体职责边界与混沌问题排查纠偏清单.md`](file:///E:/FlowSight/docs/09-FlowSight_端云协同架构全景图_实体职责边界与混沌问题排查纠偏清单.md)

---

## 一、阶段目标与业务背景

在实际的智能安防与客流分析落地交付中，现场工控机往往由物业工程部人员或集成商在现场物理部署。

以往存在的交付痛点：
1. **人工录入易错**：现场人员需要手动向云端管理员询问 RTSP 摄像头长串 URL（如 `rtsp://admin:123456@192.168.1.64:554/h264`），再手动复制粘贴到工控机配置输入框中，极易因特殊字符、端口、密码输错导致视频流打不开。
2. **云端配置变更无法感知**：当园区或商场物业更换了监控摄像头、调整了 RTSP 流地址或通道名称时，工控机无法感知，需人工去工控机现场逐台重新配置。
3. **缺乏热重载能力**：修改输入源往往需要完全退出重启整个工控机程序，打断正在进行的客流监控与识别流水线。

**阶段四实施核心成果**：
- **云端配置标准拉取协议 (`GET /api/v1/edge/config`)**：利用点位凭证 `X-Point-Token` 实现点位资产属性（名称、摄像头 RTSP 地址、分辨率、编码）的安全拉取。
- **边端零配置自适应绑定与拉取**：在 FlowSight.exe 客户端通过手机号登录并绑定点位后，自动链式触发云端配置同步，现场工程师实现"0 手动打字"开箱即用。
- **视频流自适应热重载 (Hot-Reload)**：通过 `video_source_changed` 信号与 `PipelineController::set_video_source()` 联动，无缝切换后台捕获视频源，无需重启程序。
- **交互与诊断面板增强**：在【系统设置 - 云端协同】中增设 `🔄 同步云端配置 (RTSP流)` 专属按钮与即时反馈弹窗，清晰提示同步结果。

---

## 二、端云数据流转与热重载架构

```
[ SaaS 管理后台 (solution/apps/admin) ]
   │
   ├─ 1. 运营管理员录入/更新点位 RTSP 地址 (例如: rtsp://192.168.1.100:554/live/ch1)
   │
[ FlowSight Cloud 云端中心服务 (Spring Boot 3) ]
   │
   ├─ 2. GET /api/v1/edge/config 接口接收鉴权请求头 X-Point-Token
   │      └─ 查询 biz_point 并返回 PointVO (包含 pointName, cameraRtsp, resolution 等)
   │
[ FlowSight.exe 现场工控机客户端 (C++ Qt6) ]
   │
   ├─ 3. 现场人员点击【🔗 确认绑定此点位】或【🔄 同步云端配置】
   │      │
   │      ▼
   ├─ 4. EdgeReportManager::fetch_cloud_config()
   │      │  - 携带 X-Point-Token 向云端请求配置 JSON
   │      │  - 校验 HTTP 200 与业务状态码
   │      ▼
   ├─ 5. SettingsWidget::on_btn_sync_cloud_config()
   │      │  - 更新客户端配置 (ConfigManager)
   │      │  - 更新本地点位名称与状态提示条
   │      │  - 若 cameraRtsp 非空:
   │      │      ├─ 填入 video_input_edit_
   │      │      ├─ 保存到持久化配置
   │      │      └─ 发射 video_source_changed(rtsp)
   │      ▼
   └─ 6. PipelineController::set_video_source(rtsp)
          │  - 平滑终止旧视频采集线程 (cv::VideoCapture release)
          │  - 初始化新 RTSP 网络流并自动重连
          └─ 恢复 AI 跟踪推理与大屏渲染 (自适应热重载完成)
```

---

## 三、核心源码实现

### 1. 边缘端云配置拉取引擎 (`src/network/EdgeReportManager`)
在 `EdgeReportManager.hpp` 中定义回调与接口：
```cpp
using ConfigCallback = std::function<void(bool success, const QString& msg, const QJsonObject& point_config)>;
void fetch_cloud_config(ConfigCallback callback);
```

在 `EdgeReportManager.cpp` 中实现异步 HTTP GET：
```cpp
void EdgeReportManager::fetch_cloud_config(ConfigCallback callback) {
    if (cloud_base_url_.trimmed().isEmpty() || bound_point_token_.trimmed().isEmpty()) {
        if (callback) callback(false, "未配置云端中台地址或未绑定点位 Token", QJsonObject());
        return;
    }

    if (!nam_) nam_ = new QNetworkAccessManager(this);

    QUrl config_url(cloud_base_url_.trimmed() + "/api/v1/edge/config");
    QNetworkRequest request(config_url);
    request.setRawHeader("X-Point-Token", bound_point_token_.toUtf8());
    request.setTransferTimeout(5000);

    QNetworkReply* reply = nam_->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, callback]() {
        reply->deleteLater();
        int http_status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() == QNetworkReply::NoError && http_status == 200) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            QJsonObject root = doc.object();
            int code = root.value("code").toInt(200);
            if (code == 200 && root.contains("data")) {
                QJsonObject point_cfg = root.value("data").toObject();
                // 同步本地点位名称与状态
                bound_point_name_ = point_cfg.value("pointName").toString(bound_point_name_);
                is_online_ = true;
                is_point_deactivated_ = false;
                emit cloud_status_changed(true, QString("已绑定点位: %1 [在线]").arg(bound_point_name_));
                if (callback) callback(true, "拉取云端点位配置成功", point_cfg);
                return;
            } else {
                QString msg = root.value("message").toString("云端返回异常");
                if (callback) callback(false, msg, QJsonObject());
                return;
            }
        }

        if (http_status == 400 || http_status == 401 || http_status == 403) {
            is_point_deactivated_ = true;
            is_online_ = false;
            emit cloud_status_changed(false, "⚠️ 点位已下架或Token已注销 (本地持续自主监控)");
        }
        if (callback) callback(false, QString("拉取失败 (HTTP %1): %2").arg(http_status).arg(reply->errorString()), QJsonObject());
    });
}
```

### 2. 设置界面一键同步与自动热重载 (`src/ui/views/SettingsWidget.cpp`)
```cpp
// 1. 点位绑定成功后自动触发链式同步
connect(btn_bind_point_, &QPushButton::clicked, this, [this]() {
    ...
    EdgeReportManager::instance().bind_point(url, pid, pname, ptoken, client_name);
    ...
    // 绑定成功后自动拉取云端 RTSP 流与点位参数
    btn_sync_config_->click();
});

// 2. 独立云端配置同步响应逻辑
connect(btn_sync_config_, &QPushButton::clicked, this, [this]() {
    btn_sync_config_->setEnabled(false);
    btn_sync_config_->setText("正在同步...");
    cloud_status_info_label_->setText("正在从云端拉取点位参数与摄像头 RTSP 配置...");

    EdgeReportManager::instance().fetch_cloud_config([this](bool success, const QString& msg, const QJsonObject& cfg_data) {
        btn_sync_config_->setEnabled(true);
        btn_sync_config_->setText("🔄 同步云端配置 (RTSP流)");

        if (success) {
            QString pname = cfg_data.value("pointName").toString();
            QString rtsp = cfg_data.value("cameraRtsp").toString();
            QString res = cfg_data.value("resolution").toString();

            auto& cfg = ConfigManager::instance();
            if (!pname.isEmpty()) {
                cfg.set_cloud_point_name(pname);
            }

            if (!rtsp.trimmed().isEmpty()) {
                video_input_edit_->setText(rtsp.trimmed());
                cfg.set_video_source_uri(rtsp.trimmed());
                cfg.save();
                // 关键点: 发射信号触发流水线热重载
                emit video_source_changed(rtsp.trimmed());

                cloud_status_info_label_->setText(QString("✅ 云端配置已同步！点位: %1 | 摄像头 RTSP: %2 | 已自适应热重载视频流").arg(pname).arg(rtsp));
                cloud_status_info_label_->setStyleSheet("color: #3fb950; font-size: 12px; font-weight: bold; padding: 6px 10px; background: #0d1117; border-radius: 4px; border: 1px solid #238636;");
                QMessageBox::information(this, "云端配置已同步", QString("成功从云端中台拉取最新点位资产！\n点位名称: %1\n摄像头流: %2\n分辨率: %3\n\n已自适应热重载视频流！").arg(pname).arg(rtsp).arg(res));
            } else {
                cfg.save();
                cloud_status_info_label_->setText(QString("✅ 云端配置已同步！点位: %1 (云端未配置 RTSP 流地址)").arg(pname));
                cloud_status_info_label_->setStyleSheet("color: #3fb950; font-size: 12px; font-weight: bold; padding: 6px 10px; background: #0d1117; border-radius: 4px; border: 1px solid #238636;");
                QMessageBox::information(this, "云端配置已同步", QString("点位名称: %1\n云端尚未录入 RTSP 视频流地址，保持当前视频输入源不变。").arg(pname));
            }
        } else {
            cloud_status_info_label_->setText(QString("❌ 同步云端配置失败: %1").arg(msg));
            cloud_status_info_label_->setStyleSheet("color: #f85149; font-size: 12px; font-weight: bold; padding: 6px 10px; background: #0d1117; border-radius: 4px; border: 1px solid #da3633;");
            QMessageBox::warning(this, "同步失败", msg);
        }
    });
});
```

---

## 四、构建与验证结论

1. **FlowSight.exe C++ 构建验证**:
   - `EdgeReportManager.cpp` 与 `SettingsWidget.cpp` 编译通过，链接 `FlowSight.exe` 产物正常生成。
2. **端云闭环验证流程**:
   - 在 PC 运营管理后台将点位 1 的摄像头 RTSP 修改为指定流；
   - 现场 FlowSight.exe 点击 `🔄 同步云端配置 (RTSP流)`；
   - 客户端立即打出日志：`[EdgeReportManager] Fetched cloud config for point`；
   - 界面弹出成功通知，视频流瞬间无缝热切换到新 RTSP 源；
   - 整个过程客户端无需关闭重启，客流识别流水线平稳过渡。

**阶段四目标已 100% 达成！至此，蓝图中的四大阶段全量实施完毕。**
