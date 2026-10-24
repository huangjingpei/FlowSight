package com.flowsight.cloud.service.impl;

import com.baomidou.mybatisplus.core.conditions.query.LambdaQueryWrapper;
import com.flowsight.cloud.common.JwtUtil;
import com.flowsight.cloud.dto.LoginDTO;
import com.flowsight.cloud.entity.SysUser;
import com.flowsight.cloud.mapper.SysUserMapper;
import com.flowsight.cloud.service.AuthService;
import com.flowsight.cloud.vo.LoginVO;
import com.flowsight.cloud.vo.QrCodeVO;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.security.crypto.bcrypt.BCryptPasswordEncoder;
import org.springframework.stereotype.Service;

import java.time.LocalDateTime;
import java.util.Map;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.TimeUnit;

@Service
public class AuthServiceImpl implements AuthService {

    @Autowired
    private SysUserMapper userMapper;

    @Autowired
    private JwtUtil jwtUtil;

    @Autowired(required = false)
    private StringRedisTemplate redisTemplate;

    private final BCryptPasswordEncoder passwordEncoder = new BCryptPasswordEncoder();

    // 内存兜底存储 (若 Redis 未启动或不可用)
    private final Map<String, String> localTicketStore = new ConcurrentHashMap<>();
    private final Map<String, String> localTokenStore = new ConcurrentHashMap<>();

    @Override
    public LoginVO login(LoginDTO dto) {
        SysUser user = userMapper.selectOne(new LambdaQueryWrapper<SysUser>()
                .eq(SysUser::getPhone, dto.getPhone().trim()));

        if (user == null) {
            throw new IllegalArgumentException("手机号码不存在");
        }
        if (user.getStatus() != null && user.getStatus() == 0) {
            throw new IllegalArgumentException("该账号已被管理员禁用");
        }
        if (user.getExpireTime() != null && user.getExpireTime().isBefore(LocalDateTime.now())) {
            throw new IllegalArgumentException("该商户账号 SaaS 订阅服务已到期，请联系平台管理员续约");
        }

        // 密码校验 (同时支持 BCrypt 散列与与手机号同号的明文比对兼容)
        boolean match = passwordEncoder.matches(dto.getPassword(), user.getPasswordHash())
                || dto.getPassword().equals(user.getPasswordHash())
                || dto.getPassword().equals(user.getPhone());

        if (!match) {
            throw new IllegalArgumentException("密码错误");
        }

        String token = jwtUtil.generateToken(user.getId(), user.getPhone(), user.getRole());

        LoginVO vo = new LoginVO();
        vo.setToken(token);
        vo.setUserId(user.getId());
        vo.setRole(user.getRole());
        vo.setPhone(user.getPhone());
        vo.setNickname(user.getNickname());
        vo.setCompanyName(user.getCompanyName());
        vo.setAvatar(user.getAvatar());
        return vo;
    }

    @Override
    public QrCodeVO generateQrCode() {
        String ticket = UUID.randomUUID().toString().replace("-", "");
        String status = "WAIT_SCAN";
        
        try {
            if (redisTemplate != null) {
                redisTemplate.opsForValue().set("qrcode:" + ticket, status, 120, TimeUnit.SECONDS);
            } else {
                localTicketStore.put(ticket, status);
            }
        } catch (Exception e) {
            localTicketStore.put(ticket, status);
        }

        QrCodeVO vo = new QrCodeVO();
        vo.setTicket(ticket);
        vo.setQrcodeContent("flowsight://login?ticket=" + ticket);
        vo.setStatus(status);
        return vo;
    }

    @Override
    public void scanQrCode(String ticket, Long userId) {
        try {
            if (redisTemplate != null) {
                redisTemplate.opsForValue().set("qrcode:" + ticket, "SCANNED", 120, TimeUnit.SECONDS);
            } else {
                localTicketStore.put(ticket, "SCANNED");
            }
        } catch (Exception e) {
            localTicketStore.put(ticket, "SCANNED");
        }
    }

    @Override
    public void confirmQrCode(String ticket, Long userId, Long pointId) {
        SysUser user = userMapper.selectById(userId);
        if (user == null) {
            throw new IllegalArgumentException("用户不存在");
        }
        String token = jwtUtil.generateToken(user.getId(), user.getPhone(), user.getRole());

        try {
            if (redisTemplate != null) {
                redisTemplate.opsForValue().set("qrcode:" + ticket, "CONFIRMED", 120, TimeUnit.SECONDS);
                redisTemplate.opsForValue().set("qrcode:token:" + ticket, token, 120, TimeUnit.SECONDS);
            } else {
                localTicketStore.put(ticket, "CONFIRMED");
                localTokenStore.put(ticket, token);
            }
        } catch (Exception e) {
            localTicketStore.put(ticket, "CONFIRMED");
            localTokenStore.put(ticket, token);
        }
    }

    @Override
    public QrCodeVO pollQrCode(String ticket) {
        String status = "WAIT_SCAN";
        String token = null;

        try {
            if (redisTemplate != null) {
                String val = redisTemplate.opsForValue().get("qrcode:" + ticket);
                if (val != null) status = val;
                token = redisTemplate.opsForValue().get("qrcode:token:" + ticket);
            } else {
                status = localTicketStore.getOrDefault(ticket, "EXPIRED");
                token = localTokenStore.get(ticket);
            }
        } catch (Exception e) {
            status = localTicketStore.getOrDefault(ticket, "WAIT_SCAN");
            token = localTokenStore.get(ticket);
        }

        QrCodeVO vo = new QrCodeVO();
        vo.setTicket(ticket);
        vo.setStatus(status);
        vo.setToken(token);
        return vo;
    }
}
