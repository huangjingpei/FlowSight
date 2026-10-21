package com.flowsight.cloud.controller;

import com.baomidou.mybatisplus.extension.plugins.pagination.Page;
import com.flowsight.cloud.common.Result;
import com.flowsight.cloud.service.EventService;
import com.flowsight.cloud.service.PointService;
import com.flowsight.cloud.vo.DashboardSummaryVO;
import com.flowsight.cloud.vo.EventItemVO;
import com.flowsight.cloud.vo.PointVO;
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

    @GetMapping("/points")
    public Result<List<PointVO>> listMyPoints(HttpServletRequest request) {
        Long userId = (Long) request.getAttribute("userId");
        return Result.success(pointService.listUserPoints(userId));
    }

    @GetMapping("/dashboard/summary")
    public Result<DashboardSummaryVO> getDashboardSummary(@RequestParam Long pointId, HttpServletRequest request) {
        return Result.success(eventService.getDashboardSummary(pointId));
    }

    @GetMapping("/events")
    public Result<Page<EventItemVO>> listEvents(@RequestParam Long pointId,
                                                @RequestParam(defaultValue = "1") int page,
                                                @RequestParam(defaultValue = "20") int size,
                                                HttpServletRequest request) {
        return Result.success(eventService.pageEvents(pointId, page, size));
    }
}
