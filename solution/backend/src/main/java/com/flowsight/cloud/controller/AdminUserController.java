package com.flowsight.cloud.controller;

import com.baomidou.mybatisplus.extension.plugins.pagination.Page;
import com.flowsight.cloud.common.Result;
import com.flowsight.cloud.dto.UserCreateDTO;
import com.flowsight.cloud.dto.UserUpdateDTO;
import com.flowsight.cloud.service.UserService;
import com.flowsight.cloud.vo.UserVO;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.validation.Valid;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.web.bind.annotation.*;

@RestController
@RequestMapping("/api/v1/admin/users")
public class AdminUserController {

    @Autowired
    private UserService userService;

    private void checkAdmin(HttpServletRequest request) {
        String role = (String) request.getAttribute("role");
        if (!"ROLE_ADMIN".equalsIgnoreCase(role)) {
            throw new IllegalArgumentException("无权访问：仅超级管理员可用");
        }
    }

    @GetMapping
    public Result<Page<UserVO>> listUsers(@RequestParam(defaultValue = "1") int page,
                                          @RequestParam(defaultValue = "10") int size,
                                          @RequestParam(required = false) String keyword,
                                          HttpServletRequest request) {
        checkAdmin(request);
        return Result.success(userService.pageUsers(page, size, keyword));
    }

    @PostMapping
    public Result<UserVO> createUser(@Valid @RequestBody UserCreateDTO dto, HttpServletRequest request) {
        checkAdmin(request);
        return Result.success("用户添加成功", userService.createUser(dto));
    }

    @PutMapping("/{id}")
    public Result<Void> updateUser(@PathVariable Long id, @RequestBody UserUpdateDTO dto, HttpServletRequest request) {
        checkAdmin(request);
        userService.updateUser(id, dto);
        return Result.success("修改成功", null);
    }

    @DeleteMapping("/{id}")
    public Result<Void> deleteUser(@PathVariable Long id, HttpServletRequest request) {
        checkAdmin(request);
        // 级联删除云端用户、点位及 MinIO 切片 (本地客户端数据完全自治保留)
        userService.deleteUserCascadeCloud(id);
        return Result.success("用户及云端关联数据已成功清除", null);
    }

    @GetMapping("/{id}")
    public Result<UserVO> getUser(@PathVariable Long id, HttpServletRequest request) {
        checkAdmin(request);
        return Result.success(userService.getUserById(id));
    }
}
