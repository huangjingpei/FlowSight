package com.flowsight.cloud.controller;

import com.baomidou.mybatisplus.extension.plugins.pagination.Page;
import com.flowsight.cloud.common.Result;
import com.flowsight.cloud.service.EventService;
import com.flowsight.cloud.service.PointService;
import com.flowsight.cloud.vo.DashboardSummaryVO;
import com.flowsight.cloud.vo.EventItemVO;
import com.flowsight.cloud.vo.PointVO;
import com.flowsight.cloud.vo.TrendReportVO;
import jakarta.servlet.http.HttpServletRequest;
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
}
