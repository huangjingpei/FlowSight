package com.flowsight.cloud.service.impl;

import com.baomidou.mybatisplus.core.conditions.query.LambdaQueryWrapper;
import com.baomidou.mybatisplus.extension.plugins.pagination.Page;
import com.flowsight.cloud.dto.PointCreateDTO;
import com.flowsight.cloud.dto.UserCreateDTO;
import com.flowsight.cloud.dto.UserUpdateDTO;
import com.flowsight.cloud.entity.BizPoint;
import com.flowsight.cloud.entity.SysUser;
import com.flowsight.cloud.mapper.BizPointMapper;
import com.flowsight.cloud.mapper.SysUserMapper;
import com.flowsight.cloud.service.PointService;
import com.flowsight.cloud.service.UserService;
import com.flowsight.cloud.vo.UserVO;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.security.crypto.bcrypt.BCryptPasswordEncoder;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.time.LocalDateTime;
import java.util.List;
import java.util.stream.Collectors;

@Service
public class UserServiceImpl implements UserService {

    @Autowired
    private SysUserMapper userMapper;

    @Autowired
    private BizPointMapper pointMapper;

    @Autowired
    private PointService pointService;

    private final BCryptPasswordEncoder passwordEncoder = new BCryptPasswordEncoder();

    @Override
    public Page<UserVO> pageUsers(int page, int size, String keyword) {
        Page<SysUser> userPage = new Page<>(page, size);
        LambdaQueryWrapper<SysUser> wrapper = new LambdaQueryWrapper<SysUser>()
                .and(keyword != null && !keyword.trim().isEmpty(), w -> w
                        .like(SysUser::getPhone, keyword)
                        .or().like(SysUser::getNickname, keyword)
                        .or().like(SysUser::getCompanyName, keyword)
                        .or().like(SysUser::getContactPerson, keyword))
                .orderByDesc(SysUser::getId);

        Page<SysUser> res = userMapper.selectPage(userPage, wrapper);
        Page<UserVO> voPage = new Page<>(res.getCurrent(), res.getSize(), res.getTotal());
        voPage.setRecords(res.getRecords().stream().map(this::toVO).collect(Collectors.toList()));
        return voPage;
    }

    @Override
    @Transactional(rollbackFor = Exception.class)
    public UserVO createUser(UserCreateDTO dto) {
        String phone = dto.getPhone().trim();
        Long count = userMapper.selectCount(new LambdaQueryWrapper<SysUser>().eq(SysUser::getPhone, phone));
        if (count > 0) {
            throw new IllegalArgumentException("该手机号码已注册");
        }

        // 默认密码与手机号同号
        String rawPassword = (dto.getPassword() != null && !dto.getPassword().trim().isEmpty())
                ? dto.getPassword().trim() : phone;

        SysUser user = new SysUser();
        user.setPhone(phone);
        user.setPasswordHash(passwordEncoder.encode(rawPassword));
        user.setRole("ROLE_USER");
        user.setNickname(dto.getNickname() != null && !dto.getNickname().trim().isEmpty() 
                ? dto.getNickname().trim() 
                : "商户_" + phone.substring(Math.max(0, phone.length() - 4)));
        user.setCompanyName(dto.getCompanyName() != null && !dto.getCompanyName().trim().isEmpty() 
                ? dto.getCompanyName().trim() 
                : user.getNickname());
        user.setContactPerson(dto.getContactPerson() != null ? dto.getContactPerson().trim() : "");
        user.setPointQuota(dto.getPointQuota() != null && dto.getPointQuota() > 0 ? dto.getPointQuota() : 5);
        user.setExpireTime(dto.getExpireTime() != null ? dto.getExpireTime() : LocalDateTime.now().plusYears(1));
        user.setStatus(1);

        userMapper.insert(user);

        // 若指定了初始点位名称，自动创建初始点位
        if (dto.getInitialPointName() != null && !dto.getInitialPointName().trim().isEmpty()) {
            PointCreateDTO pointDTO = new PointCreateDTO();
            pointDTO.setUserId(user.getId());
            pointDTO.setPointName(dto.getInitialPointName().trim());
            pointDTO.setAddress(user.getCompanyName() + " 主入口");
            pointService.createPoint(pointDTO);
        }

        return toVO(user);
    }

    @Override
    public void updateUser(Long id, UserUpdateDTO dto) {
        SysUser user = userMapper.selectById(id);
        if (user == null) {
            throw new IllegalArgumentException("用户不存在");
        }
        if (dto.getNickname() != null) user.setNickname(dto.getNickname());
        if (dto.getCompanyName() != null) user.setCompanyName(dto.getCompanyName());
        if (dto.getContactPerson() != null) user.setContactPerson(dto.getContactPerson());
        if (dto.getPointQuota() != null && dto.getPointQuota() > 0) user.setPointQuota(dto.getPointQuota());
        if (dto.getExpireTime() != null) user.setExpireTime(dto.getExpireTime());
        if (dto.getStatus() != null) user.setStatus(dto.getStatus());
        if (dto.getPassword() != null && !dto.getPassword().trim().isEmpty()) {
            user.setPasswordHash(passwordEncoder.encode(dto.getPassword().trim()));
        }
        userMapper.updateById(user);
    }

    @Override
    @Transactional(rollbackFor = Exception.class)
    public void deleteUserCascadeCloud(Long id) {
        SysUser user = userMapper.selectById(id);
        if (user == null) return;

        // 1. 查找名下全部点位并逐一清理云端数据与切片 (本地客户端数据完全自治保留)
        List<BizPoint> points = pointMapper.selectList(new LambdaQueryWrapper<BizPoint>().eq(BizPoint::getUserId, id));
        for (BizPoint pt : points) {
            pointService.deletePointCascadeCloud(pt.getId());
        }

        // 2. 删除云端用户
        userMapper.deleteById(id);
    }

    @Override
    public UserVO getUserById(Long id) {
        SysUser user = userMapper.selectById(id);
        return user != null ? toVO(user) : null;
    }

    private UserVO toVO(SysUser u) {
        UserVO vo = new UserVO();
        vo.setId(u.getId());
        vo.setPhone(u.getPhone());
        vo.setRole(u.getRole());
        vo.setNickname(u.getNickname());
        vo.setCompanyName(u.getCompanyName());
        vo.setContactPerson(u.getContactPerson());
        vo.setPointQuota(u.getPointQuota());
        vo.setExpireTime(u.getExpireTime());
        vo.setAvatar(u.getAvatar());
        vo.setStatus(u.getStatus());
        vo.setCreateTime(u.getCreateTime());

        Long ptCount = pointMapper.selectCount(new LambdaQueryWrapper<BizPoint>().eq(BizPoint::getUserId, u.getId()));
        vo.setPointCount(ptCount != null ? ptCount.intValue() : 0);
        return vo;
    }
}
