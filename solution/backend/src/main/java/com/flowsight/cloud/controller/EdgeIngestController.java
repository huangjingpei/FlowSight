package com.flowsight.cloud.controller;

import com.flowsight.cloud.common.Result;
import com.flowsight.cloud.dto.DeviceLoginDTO;
import com.flowsight.cloud.dto.EdgeBatchReportDTO;
import com.flowsight.cloud.dto.EdgeEventDTO;
import com.flowsight.cloud.dto.EdgeHeartbeatDTO;
import com.flowsight.cloud.dto.PairingRequestDTO;
import com.flowsight.cloud.dto.PointUpdateDTO;
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
     * 极简接入：边缘工控机凭 6 位配对码一键接入并换取长效 Token
     */
    @PostMapping("/auth/pair-with-code")
    public Result<DeviceLoginVO> pairWithCode(@Valid @RequestBody PairingRequestDTO dto) {
        return Result.success("配对成功，已激活点位", pointService.pairWithCode(dto));
    }

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

    /**
     * 现场快照预览：边缘上传当前带 ROI 绊线的现场缩略图
     */
    @PostMapping("/snapshot")
    public Result<String> uploadSnapshot(@RequestHeader(value = "X-Point-Token", required = false) String token,
                                         @RequestParam("file") MultipartFile file) {
        BizPoint point = authenticateEdge(token);
        String snapshotUrl = eventService.uploadEdgeSnapshot(point.getId(), file);
        return Result.success("现场快照已更新", snapshotUrl);
    }

    @PostMapping("/heartbeat")
    public Result<Void> heartbeat(@RequestHeader(value = "X-Point-Token", required = false) String token,
                                  @RequestBody EdgeHeartbeatDTO dto) {
        BizPoint point = authenticateEdge(token);
        eventService.recordHeartbeat(point, dto);
        return Result.success(null);
    }

    @GetMapping("/config")
    public Result<PointVO> getEdgeConfig(@RequestHeader(value = "X-Point-Token", required = false) String token) {
        BizPoint point = authenticateEdge(token);
        return Result.success(pointService.getPointById(point.getId()));
    }

    @PutMapping("/config")
    public Result<Void> updateEdgeConfig(@RequestHeader(value = "X-Point-Token", required = false) String token,
                                         @RequestBody PointUpdateDTO dto) {
        BizPoint point = authenticateEdge(token);
        pointService.updatePoint(point.getId(), dto);
        return Result.success("边缘端点位配置已成功双向同步至云端", null);
    }

    /**
     * 边缘点位独占绑定请求 (包含排他锁核验，防止多机重复抢占同一通道)
     */
    @PostMapping("/bind")
    public Result<DeviceLoginVO> bindEdgePoint(@RequestHeader(value = "X-Point-Token", required = false) String token,
                                               @RequestBody(required = false) DeviceLoginDTO dto) {
        BizPoint point = authenticateEdge(token);
        String clientName = (dto != null && dto.getClientName() != null) ? dto.getClientName() : "FlowSight-Node";
        boolean force = (dto != null && Boolean.TRUE.equals(dto.getForce()));
        DeviceLoginVO vo = pointService.bindPoint(point.getId(), clientName, force);
        return Result.success("点位已成功独占锁定并绑定", vo);
    }

    /**
     * 边缘节点主动解绑与释放点位 (下线握手，杜绝换绑双在线幽灵状态)
     */
    @PostMapping("/unbind")
    public Result<Void> unbindEdgePoint(@RequestHeader(value = "X-Point-Token", required = false) String token,
                                        @RequestBody(required = false) DeviceLoginDTO dto) {
        BizPoint point = authenticateEdge(token);
        String clientName = (dto != null && dto.getClientName() != null) ? dto.getClientName() : "FlowSight-Node";
        pointService.unbindPoint(point.getId(), clientName);
        return Result.success("点位已成功解绑下线并释放独占锁", null);
    }
}
