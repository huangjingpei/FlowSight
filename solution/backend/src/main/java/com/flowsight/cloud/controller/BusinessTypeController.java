package com.flowsight.cloud.controller;

import com.baomidou.mybatisplus.extension.plugins.pagination.Page;
import com.flowsight.cloud.common.Result;
import com.flowsight.cloud.dto.BusinessTypeDTO;
import com.flowsight.cloud.service.BusinessTypeService;
import com.flowsight.cloud.vo.BusinessTypeOptionVO;
import com.flowsight.cloud.vo.BusinessTypeVO;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.validation.Valid;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.web.bind.annotation.*;

import java.util.List;

@RestController
@RequestMapping("/api/v1/business-types")
public class BusinessTypeController {

    @Autowired
    private BusinessTypeService businessTypeService;

    private void checkAdmin(HttpServletRequest request) {
        String role = (String) request.getAttribute("role");
        if (!"ROLE_ADMIN".equalsIgnoreCase(role)) {
            throw new IllegalArgumentException("无权操作：业务类型配置仅超级管理员可用");
        }
    }

    /**
     * 分页查询业务类型列表 (后台管理用)
     */
    @GetMapping
    public Result<Page<BusinessTypeVO>> pageBusinessTypes(@RequestParam(defaultValue = "1") int page,
                                                          @RequestParam(defaultValue = "10") int size,
                                                          @RequestParam(required = false) String keyword,
                                                          HttpServletRequest request) {
        checkAdmin(request);
        return Result.success(businessTypeService.pageBusinessTypes(page, size, keyword));
    }

    /**
     * 获取所有启用的业务模式快选清单 (供点位创建/编辑下拉框动态读取)
     */
    @GetMapping("/options")
    public Result<List<BusinessTypeOptionVO>> listActiveOptions() {
        return Result.success(businessTypeService.listActiveOptions());
    }

    /**
     * 获取单条业务类型详情
     */
    @GetMapping("/{id}")
    public Result<BusinessTypeVO> getBusinessType(@PathVariable Long id, HttpServletRequest request) {
        checkAdmin(request);
        return Result.success(businessTypeService.getById(id));
    }

    /**
     * 新增业务类型
     */
    @PostMapping
    public Result<BusinessTypeVO> createBusinessType(@Valid @RequestBody BusinessTypeDTO dto, HttpServletRequest request) {
        checkAdmin(request);
        return Result.success("业务类型创建成功", businessTypeService.createBusinessType(dto));
    }

    /**
     * 更新业务类型
     */
    @PutMapping("/{id}")
    public Result<Void> updateBusinessType(@PathVariable Long id, @Valid @RequestBody BusinessTypeDTO dto, HttpServletRequest request) {
        checkAdmin(request);
        businessTypeService.updateBusinessType(id, dto);
        return Result.success("业务类型更新成功", null);
    }

    /**
     * 删除业务类型 (附带强引用检查)
     */
    @DeleteMapping("/{id}")
    public Result<Void> deleteBusinessType(@PathVariable Long id, HttpServletRequest request) {
        checkAdmin(request);
        businessTypeService.deleteBusinessType(id);
        return Result.success("业务类型已成功删除", null);
    }

    /**
     * 快捷切换业务模式启用/停用状态
     */
    @PatchMapping("/{id}/status")
    public Result<Void> toggleStatus(@PathVariable Long id, @RequestParam Integer status, HttpServletRequest request) {
        checkAdmin(request);
        businessTypeService.toggleStatus(id, status);
        return Result.success("业务状态已变更", null);
    }
}
