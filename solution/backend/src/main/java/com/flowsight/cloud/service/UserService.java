package com.flowsight.cloud.service;

import com.baomidou.mybatisplus.extension.plugins.pagination.Page;
import com.flowsight.cloud.dto.UserCreateDTO;
import com.flowsight.cloud.dto.UserUpdateDTO;
import com.flowsight.cloud.vo.UserVO;

public interface UserService {
    Page<UserVO> pageUsers(int page, int size, String keyword);
    UserVO createUser(UserCreateDTO dto);
    void updateUser(Long id, UserUpdateDTO dto);
    void deleteUserCascadeCloud(Long id);
    UserVO getUserById(Long id);
    void changePassword(Long userId, String oldPassword, String newPassword);
    void setDefaultPoint(Long userId, Long pointId);
}
