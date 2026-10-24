package com.flowsight.cloud.controller;

import com.flowsight.cloud.common.Result;
import com.flowsight.cloud.dto.DeviceLoginDTO;
import com.flowsight.cloud.dto.EdgeBatchReportDTO;
import com.flowsight.cloud.dto.EdgeEventDTO;
import com.flowsight.cloud.dto.EdgeHeartbeatDTO;
import com.flowsight.cloud.entity.BizPoint;
import com.flowsight.cloud.service.EventService;
import com.flowsight.cloud.service.PointService;
import com.flowsight.cloud.vo.DeviceLoginVO;
import com.flowsight.cloud.vo.PointVO;
import jakarta.validation.Valid;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.multipart.MultipartFile;

@RestController
@RequestMapping("/api/v1/edge")
public class EdgeIngestController {

    @Autowired
    private PointService pointService;

    @Autowired
    private EventService eventService;

    /**
     * 双密码体系：边缘设备专属身份接入认证 (密码 2)
     */
    @PostMapping("/auth/device-login")
    public Result<DeviceLoginVO> deviceLogin(@Valid @RequestBody DeviceLoginDTO dto) {
        return Result.success("边缘设备认证就绪", pointService.loginWithDeviceCredentials(dto));
    }

    private BizPoint authenticateEdge(String token) {
        if (token == null || token.trim().isEmpty()) {
            throw new IllegalArgumentException("未授权: 缺少 X-Point-Token 请求头");
        }
        BizPoint point = pointService.getPointByToken(token);
        if (point == null) {
            throw new IllegalArgumentException("未授权: 无效的 Point Token");
        }
        return point;
    }

    @PostMapping("/event")
    public Result<Void> ingestEvent(@RequestHeader(value = "X-Point-Token", required = false) String token,
                                    @RequestBody EdgeEventDTO dto) {
        BizPoint point = authenticateEdge(token);
        eventService.recordEdgeEvent(point.getId(), dto);
        return Result.success(null);
    }

    @PostMapping("/metrics/batch")
    public Result<String> ingestBatchMetrics(@RequestHeader(value = "X-Point-Token", required = false) String token,
                                             @RequestBody EdgeBatchReportDTO dto) {
        BizPoint point = authenticateEdge(token);
        eventService.recordEdgeBatchReport(point.getId(), dto);
        return Result.success("批量指标归档成功", dto.getBatchUuid());
    }

    @PostMapping("/clip/upload")
    public Result<String> uploadClip(@RequestHeader(value = "X-Point-Token", required = false) String token,
                                     @RequestParam(required = false) Integer trackId,
                                     @RequestParam(required = false) Long timestamp,
                                     @RequestParam("file") MultipartFile file) {
        BizPoint point = authenticateEdge(token);
        String clipUrl = eventService.uploadEdgeClip(point.getId(), trackId, timestamp, file);
        return Result.success("切片上传就绪", clipUrl);
    }

    @PostMapping("/heartbeat")
    public Result<Void> heartbeat(@RequestHeader(value = "X-Point-Token", required = false) String token,
                                  @RequestBody EdgeHeartbeatDTO dto) {
        BizPoint point = authenticateEdge(token);
        eventService.recordHeartbeat(point.getId(), dto);
        return Result.success(null);
    }

    @GetMapping("/config")
    public Result<PointVO> getEdgeConfig(@RequestHeader(value = "X-Point-Token", required = false) String token) {
        BizPoint point = authenticateEdge(token);
        return Result.success(pointService.getPointById(point.getId()));
    }
}
