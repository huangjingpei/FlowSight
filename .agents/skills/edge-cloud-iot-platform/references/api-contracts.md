# 边云标准通信协议契约 (API Contracts)

本文件规范边缘端与云端、云端与前端应用的标准 HTTP RESTful 协议接口。

---

## 1. 边缘端与云端交互接口 (Edge Ingestion)

边缘设备接入时，必须在 HTTP Header 中提供分配的认证令牌：
- Header 名称：`X-Point-Token`
- 鉴权逻辑：云端通过 SHA-256 散列后与 `tb_point.token_hash` 进行匹配，校验通过后取得关联的 `point_id` 与 `tenant_id`。

### 1.1 边缘心跳上报
- **URL**: `POST /api/v1/edge/heartbeat`
- **Headers**:
  - `X-Point-Token`: `pt_xxxxxxxxxxxxxxxx`
  - `Content-Type`: `application/json`
- **Request Body**:
  ```json
  {
    "appVersion": "v1.2.0",
    "cpuUsage": 18.5,
    "memoryUsage": 42.1,
    "fps": 25.0
  }
  ```
- **Response Body**:
  ```json
  {
    "code": 200,
    "message": "success",
    "data": {
      "ack": true,
      "serverTime": "2026-10-21T18:00:00"
    }
  }
  ```

### 1.2 客流指标数据实时上报
- **URL**: `POST /api/v1/edge/metrics/flow`
- **Headers**:
  - `X-Point-Token`: `pt_xxxxxxxxxxxxxxxx`
- **Request Body**:
  ```json
  {
    "statDate": "2026-10-21",
    "statHour": 18,
    "inIncrement": 5,
    "outIncrement": 3,
    "maleIncrement": 3,
    "femaleIncrement": 2,
    "childIncrement": 1,
    "adultIncrement": 4,
    "seniorIncrement": 0
  }
  ```
- **Response Body**:
  ```json
  {
    "code": 200,
    "message": "success",
    "data": true
  }
  ```

### 1.3 视频切片上传与元数据登记
- **URL**: `POST /api/v1/edge/slice/upload`
- **Headers**:
  - `X-Point-Token`: `pt_xxxxxxxxxxxxxxxx`
  - `Content-Type`: `multipart/form-data`
- **Form Parameters**:
  - `file`: 二进制 MP4 文件
  - `sliceId`: `clip_point1_20261021_180015`
  - `eventType`: `PASS`
  - `startTime`: `2026-10-21 18:00:15`
  - `endTime`: `2026-10-21 18:00:25`
  - `durationSeconds`: 10
  - `inCount`: 2
  - `outCount`: 1
- **Response Body**:
  ```json
  {
    "code": 200,
    "message": "success",
    "data": {
      "sliceId": "clip_point1_20261021_180015",
      "storagePath": "clips/point_1/clip_point1_20261021_180015.mp4"
    }
  }
  ```

---

## 2. 移动端 H5 决策看板接口 (H5 Dashboard)

所有移动端接口需在 Header 中传递商户登录颁发的 JWT 凭证：
- Header 名称：`Authorization: Bearer <jwt_token>`

### 2.1 获取商户可用点位列表
- **URL**: `GET /api/v1/h5/dashboard/points`
- **Response Body**:
  ```json
  {
    "code": 200,
    "message": "success",
    "data": [
      {
        "id": 1,
        "pointCode": "POINT_001",
        "name": "西门客流入口",
        "isOnline": 1,
        "lastHeartbeatAt": "2026-10-21T18:00:00"
      }
    ]
  }
  ```

### 2.2 获取点位今日 KPI 与画像汇总
- **URL**: `GET /api/v1/h5/dashboard/summary?pointId=1`
- **Response Body**:
  ```json
  {
    "code": 200,
    "message": "success",
    "data": {
      "pointId": 1,
      "pointName": "西门客流入口",
      "statDate": "2026-10-21",
      "kpi": {
        "totalIn": 1248,
        "totalOut": 1102,
        "currentStay": 146
      },
      "demographics": {
        "male": 680,
        "female": 568,
        "child": 120,
        "adult": 980,
        "senior": 148
      },
      "hourlyTrends": [
        { "hour": 8, "inCount": 35, "outCount": 5 },
        { "hour": 9, "inCount": 120, "outCount": 20 },
        { "hour": 18, "inCount": 210, "outCount": 180 }
      ]
    }
  }
  ```

### 2.3 获取点位视频事件短切片列表 (包含时效预签名播放 URL)
- **URL**: `GET /api/v1/h5/dashboard/slices?pointId=1`
- **Response Body**:
  ```json
  {
    "code": 200,
    "message": "success",
    "data": [
      {
        "sliceId": "clip_point1_20261021_180015",
        "eventType": "PASS",
        "startTime": "2026-10-21 18:00:15",
        "durationSeconds": 10,
        "inCount": 2,
        "outCount": 1,
        "fileSizeBytes": 1485920,
        "playUrl": "http://minio.cloud.com:9000/flowsight-clips/clips/point_1/clip_point1_20261021_180015.mp4?X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Credential=...&X-Amz-Expires=7200..."
      }
    ]
  }
  ```
