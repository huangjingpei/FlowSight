package com.flowsight.cloud.controller;

import com.flowsight.cloud.common.Result;
import com.flowsight.cloud.dto.SystemConfigUpdateDTO;
import com.flowsight.cloud.entity.SysSystemConfig;
import com.flowsight.cloud.service.SystemConfigService;
import com.flowsight.cloud.vo.SystemFeaturesVO;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.validation.Valid;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.web.bind.annotation.*;

import java.util.List;

@RestController
@RequestMapping("/api/v1")
public class SystemConfigController {

    @Autowired
    private SystemConfigService systemConfigService;

    private void checkAdmin(HttpServletRequest request) {
        String role = (String) request.getAttribute("role");
        if (!"ROLE_ADMIN".equalsIgnoreCase(role)) {
            throw new IllegalArgumentException("无权访问：仅超级管理员可用");
        }
    }

    /**
     * 公开系统特性与能力矩阵查询 (供 H5 / Web 等前端自适应展示，无需强制超管权限)
     */
    @GetMapping("/system/features")
    public Result<SystemFeaturesVO> getSystemFeatures() {
        return Result.success("系统能力矩阵就绪", systemConfigService.getSystemFeatures());
    }

    /**
     * 超级管理员获取全部系统配置项列表
     */
    @GetMapping("/admin/config")
    public Result<List<SysSystemConfig>> listConfigs(HttpServletRequest request) {
        checkAdmin(request);
        return Result.success(systemConfigService.listConfigs());
    }

    /**
     * 超级管理员更新指定系统配置项
     */
    @PostMapping("/admin/config/update")
    public Result<Void> updateConfig(@Valid @RequestBody SystemConfigUpdateDTO dto, HttpServletRequest request) {
        checkAdmin(request);
        systemConfigService.updateConfig(dto.getConfigKey(), dto.getConfigValue());
        return Result.success("系统配置已更新并实时生效", null);
    }
}
