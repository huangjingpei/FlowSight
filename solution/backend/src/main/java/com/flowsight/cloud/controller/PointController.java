package com.flowsight.cloud.controller;

import com.baomidou.mybatisplus.extension.plugins.pagination.Page;
import com.flowsight.cloud.common.Result;
import com.flowsight.cloud.dto.PointCreateDTO;
import com.flowsight.cloud.dto.PointUpdateDTO;
import com.flowsight.cloud.service.PointService;
import com.flowsight.cloud.vo.PointVO;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.validation.Valid;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.web.bind.annotation.*;

import java.util.List;

@RestController
@RequestMapping("/api/v1/points")
public class PointController {

    @Autowired
    private PointService pointService;

    @GetMapping
    public Result<Page<PointVO>> listPoints(@RequestParam(defaultValue = "1") int page,
                                            @RequestParam(defaultValue = "10") int size,
                                            @RequestParam(required = false) Long userId,
                                            @RequestParam(required = false) String keyword,
                                            @RequestParam(required = false) String bizType,
                                            HttpServletRequest request) {
        String role = (String) request.getAttribute("role");
        Long currentUserId = (Long) request.getAttribute("userId");

        // 若不是管理员，强制只能查自己名下的点位
        Long targetUserId = "ROLE_ADMIN".equalsIgnoreCase(role) ? userId : currentUserId;
        return Result.success(pointService.pagePoints(page, size, targetUserId, keyword, bizType));
    }

    @GetMapping("/my")
    public Result<List<PointVO>> listMyPoints(HttpServletRequest request) {
        Long currentUserId = (Long) request.getAttribute("userId");
        return Result.success(pointService.listUserPoints(currentUserId));
    }

    @PostMapping
    public Result<PointVO> createPoint(@Valid @RequestBody PointCreateDTO dto, HttpServletRequest request) {
        String role = (String) request.getAttribute("role");
        Long currentUserId = (Long) request.getAttribute("userId");

        if (!"ROLE_ADMIN".equalsIgnoreCase(role) || dto.getUserId() == null) {
            dto.setUserId(currentUserId);
        }
        return Result.success("点位创建成功", pointService.createPoint(dto));
    }

    @PutMapping("/{id}")
    public Result<Void> updatePoint(@PathVariable Long id, @RequestBody PointUpdateDTO dto) {
        pointService.updatePoint(id, dto);
        return Result.success("点位更新成功", null);
    }

    @DeleteMapping("/{id}")
    public Result<Void> deletePoint(@PathVariable Long id) {
        // 级联清理云端点位及 MinIO 切片 (本地客户端数据完全自治保留)
        pointService.deletePointCascadeCloud(id);
        return Result.success("点位及云端切片已成功清除", null);
    }

    @GetMapping("/{id}")
    public Result<PointVO> getPoint(@PathVariable Long id) {
        return Result.success(pointService.getPointById(id));
    }

    @PostMapping("/{id}/token/refresh")
    public Result<String> refreshToken(@PathVariable Long id) {
        return Result.success("Token 已重置刷新", pointService.refreshToken(id));
    }
}
