package com.flowsight.cloud.controller;

import com.baomidou.mybatisplus.extension.plugins.pagination.Page;
import com.flowsight.cloud.common.Result;
import com.flowsight.cloud.dto.ChangePasswordDTO;
import com.flowsight.cloud.dto.ResetDevicePasswordDTO;
import com.flowsight.cloud.service.EventService;
import com.flowsight.cloud.service.PointService;
import com.flowsight.cloud.service.UserService;
import com.flowsight.cloud.vo.DashboardSummaryVO;
import com.flowsight.cloud.vo.EventItemVO;
import com.flowsight.cloud.vo.PairingCodeVO;
import com.flowsight.cloud.vo.PointCredentialVO;
import com.flowsight.cloud.vo.PointVO;
import com.flowsight.cloud.vo.TrendReportVO;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.validation.Valid;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.web.bind.annotation.*;

import java.util.List;

@RestController
@RequestMapping("/api/v1/h5")
public class H5DashboardController {

    @Autowired
    private PointService pointService;

    @Autowired
    private EventService eventService;

    @Autowired
    private UserService userService;

    private void checkPointOwnership(Long pointId, HttpServletRequest request) {
        String role = (String) request.getAttribute("role");
        if ("ROLE_ADMIN".equalsIgnoreCase(role)) return;
        Long userId = (Long) request.getAttribute("userId");
        PointVO point = pointService.getPointById(pointId);
        if (point == null || !userId.equals(point.getUserId())) {
            throw new IllegalArgumentException("无权查看该监控点位：该点位不属于当前商户客户或已在云端被管理员移除");
        }
    }

    @GetMapping("/points")
    public Result<List<PointVO>> listMyPoints(HttpServletRequest request) {
        Long userId = (Long) request.getAttribute("userId");
        return Result.success(pointService.listUserPoints(userId));
    }

    @GetMapping("/dashboard/summary")
    public Result<DashboardSummaryVO> getDashboardSummary(@RequestParam Long pointId, HttpServletRequest request) {
        checkPointOwnership(pointId, request);
        return Result.success(eventService.getDashboardSummary(pointId));
    }

    @GetMapping("/dashboard/trends")
    public Result<TrendReportVO> getDashboardTrends(@RequestParam Long pointId,
                                                    @RequestParam(defaultValue = "hour") String dimension,
                                                    @RequestParam(defaultValue = "7") Integer days,
                                                    HttpServletRequest request) {
        checkPointOwnership(pointId, request);
        return Result.success(eventService.getTrendReport(pointId, dimension, days));
    }

    @GetMapping("/events")
    public Result<Page<EventItemVO>> listEvents(@RequestParam(required = false) Long pointId,
                                                @RequestParam(required = false) String startDate,
                                                @RequestParam(required = false) String endDate,
                                                @RequestParam(required = false) String direction,
                                                @RequestParam(defaultValue = "1") int page,
                                                @RequestParam(defaultValue = "10") int size,
                                                HttpServletRequest request) {
        Long userId = (Long) request.getAttribute("userId");
        if (pointId != null && pointId > 0) {
            checkPointOwnership(pointId, request);
        }
        return Result.success(eventService.pageEvents(userId, pointId, startDate, endDate, direction, page, size));
    }

    @PostMapping("/user/password")
    public Result<Void> changePassword(@Valid @RequestBody ChangePasswordDTO dto, HttpServletRequest request) {
        Long userId = (Long) request.getAttribute("userId");
        userService.changePassword(userId, dto.getOldPassword(), dto.getNewPassword());
        return Result.success("密码修改成功，请妥善保管新密码", null);
    }

    /**
     * 双密码体系：商户获取名下点位的边缘接入参数与密码状态 (密码 2)
     */
    @GetMapping("/points/{pointId}/credential")
    public Result<PointCredentialVO> getPointCredential(@PathVariable Long pointId, HttpServletRequest request) {
        checkPointOwnership(pointId, request);
        Long userId = (Long) request.getAttribute("userId");
        return Result.success(pointService.getPointCredential(pointId, userId));
    }

    /**
     * 极简接入：商户在移动端一键生成 6 位临时配对码 (5分钟有效)
     */
    @PostMapping("/points/{pointId}/pairing-code")
    public Result<PairingCodeVO> generatePairingCode(@PathVariable Long pointId, HttpServletRequest request) {
        checkPointOwnership(pointId, request);
        Long userId = (Long) request.getAttribute("userId");
        return Result.success("配对码已生成，请在现场工控机输入", pointService.generatePairingCode(pointId, userId));
    }

    /**
     * 双密码体系：商户重置名下边缘设备专属接入密码 (密码 2)
     */
    @PostMapping("/points/{pointId}/device-password")
    public Result<PointCredentialVO> resetDevicePassword(@PathVariable Long pointId,
                                                         @Valid @RequestBody ResetDevicePasswordDTO dto,
                                                         HttpServletRequest request) {
        checkPointOwnership(pointId, request);
        Long userId = (Long) request.getAttribute("userId");
        return Result.success("边缘设备接入密码重置成功，已自动切断旧设备通信凭证",
                pointService.resetDevicePassword(pointId, userId, dto));
    }
}
