package com.flowsight.cloud.service.impl;

import com.baomidou.mybatisplus.core.conditions.query.LambdaQueryWrapper;
import com.baomidou.mybatisplus.extension.plugins.pagination.Page;
import com.flowsight.cloud.dto.DeviceLoginDTO;
import com.flowsight.cloud.dto.PairingRequestDTO;
import com.flowsight.cloud.dto.PointCreateDTO;
import com.flowsight.cloud.dto.PointUpdateDTO;
import com.flowsight.cloud.dto.ResetDevicePasswordDTO;
import com.flowsight.cloud.entity.BizDeviceHeartbeat;
import com.flowsight.cloud.entity.BizPassageEvent;
import com.flowsight.cloud.entity.BizPoint;
import com.flowsight.cloud.entity.BizStatsHourly;
import com.flowsight.cloud.entity.SysUser;
import com.flowsight.cloud.mapper.*;
import com.flowsight.cloud.service.MinioService;
import com.flowsight.cloud.service.PointService;
import com.flowsight.cloud.vo.DeviceLoginVO;
import com.flowsight.cloud.vo.PairingCodeVO;
import com.flowsight.cloud.vo.PointCredentialVO;
import com.flowsight.cloud.vo.PointVO;
import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.security.crypto.bcrypt.BCryptPasswordEncoder;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.time.LocalDate;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.time.temporal.ChronoUnit;
import java.util.List;
import java.util.Map;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;
import java.util.stream.Collectors;

@Slf4j
@Service
public class PointServiceImpl implements PointService {

    @Autowired
    private BizPointMapper pointMapper;

    @Autowired
    private SysUserMapper userMapper;

    @Autowired
    private BizPassageEventMapper eventMapper;

    @Autowired
    private BizStatsHourlyMapper statsMapper;

    @Autowired
    private BizDeviceHeartbeatMapper heartbeatMapper;

    @Autowired
    private MinioService minioService;

    private final BCryptPasswordEncoder passwordEncoder = new BCryptPasswordEncoder();

    @Override
    public Page<PointVO> pagePoints(int page, int size, Long userId, String keyword, String bizType) {
        Page<BizPoint> pointPage = new Page<>(page, size);
        LambdaQueryWrapper<BizPoint> wrapper = new LambdaQueryWrapper<BizPoint>()
                .eq(userId != null, BizPoint::getUserId, userId)
                .eq(bizType != null && !bizType.trim().isEmpty(), BizPoint::getBizType, bizType)
                .and(keyword != null && !keyword.trim().isEmpty(), w -> w
                        .like(BizPoint::getPointName, keyword)
                        .or().like(BizPoint::getAddress, keyword)
                        .or().like(BizPoint::getPointCode, keyword))
                .orderByDesc(BizPoint::getId);

        Page<BizPoint> res = pointMapper.selectPage(pointPage, wrapper);
        Page<PointVO> voPage = new Page<>(res.getCurrent(), res.getSize(), res.getTotal());
        voPage.setRecords(res.getRecords().stream().map(this::toVO).collect(Collectors.toList()));
        return voPage;
    }

    @Override
    public List<PointVO> listUserPoints(Long userId) {
        SysUser user = userMapper.selectById(userId);
        LambdaQueryWrapper<BizPoint> wrapper = new LambdaQueryWrapper<BizPoint>().orderByDesc(BizPoint::getId);
        if (user != null && !"ROLE_ADMIN".equals(user.getRole())) {
            wrapper.eq(BizPoint::getUserId, userId);
        }
        List<BizPoint> list = pointMapper.selectList(wrapper);
        return list.stream().map(this::toVO).collect(Collectors.toList());
    }

    @Override
    public PointVO createPoint(PointCreateDTO dto) {
        SysUser user = userMapper.selectById(dto.getUserId());
        if (user == null) {
            throw new IllegalArgumentException("所属商户客户不存在");
        }
        Long existingCount = pointMapper.selectCount(new LambdaQueryWrapper<BizPoint>().eq(BizPoint::getUserId, dto.getUserId()));
        int quota = user.getPointQuota() != null ? user.getPointQuota() : 5;
        if (existingCount >= quota) {
            String clientName = user.getCompanyName() != null && !user.getCompanyName().isEmpty() ? user.getCompanyName() : user.getNickname();
            throw new IllegalArgumentException("客户【" + clientName + "】名下点位数已达配额上限 (" + quota + " 个)，无法继续新增！");
        }

        BizPoint point = new BizPoint();
        point.setUserId(dto.getUserId());
        point.setPointName(dto.getPointName());

        String targetCode = dto.getPointCode() != null ? dto.getPointCode().trim() : "";
        if (targetCode.isEmpty()) {
            targetCode = generateUniquePointCode(dto.getBizType());
        } else {
            // 严格全局防重分配校验
            Long dupCount = pointMapper.selectCount(new LambdaQueryWrapper<BizPoint>()
                    .eq(BizPoint::getPointCode, targetCode));
            if (dupCount != null && dupCount > 0) {
                throw new IllegalArgumentException("点位编码【" + targetCode + "】已被使用，请勿重复分配！");
            }
        }
        point.setPointCode(targetCode);
        String devPwd = (dto.getDevicePassword() != null && !dto.getDevicePassword().trim().isEmpty())
                ? dto.getDevicePassword().trim() : "Edge@123456";
        point.setDevicePasswordHash(passwordEncoder.encode(devPwd));
        point.setAddress(dto.getAddress() != null ? dto.getAddress() : "");
        point.setCameraRtsp(dto.getCameraRtsp() != null ? dto.getCameraRtsp() : "");
        point.setResolution(dto.getResolution() != null ? dto.getResolution() : "1920x1080");
        point.setBizType(dto.getBizType() != null && !dto.getBizType().trim().isEmpty() ? dto.getBizType().trim().toUpperCase() : "PEDESTRIAN");
        point.setStreamStatus("OFFLINE");
        point.setPointToken("ptk_" + UUID.randomUUID().toString().replace("-", ""));

        pointMapper.insert(point);
        return toVO(point);
    }

    @Override
    public void updatePoint(Long id, PointUpdateDTO dto) {
        BizPoint point = pointMapper.selectById(id);
        if (point == null) {
            throw new IllegalArgumentException("点位不存在");
        }
        if (dto.getPointName() != null) point.setPointName(dto.getPointName());
        if (dto.getPointCode() != null && !dto.getPointCode().trim().isEmpty()) {
            String newCode = dto.getPointCode().trim();
            if (!newCode.equalsIgnoreCase(point.getPointCode())) {
                Long dupCount = pointMapper.selectCount(new LambdaQueryWrapper<BizPoint>()
                        .eq(BizPoint::getPointCode, newCode)
                        .ne(BizPoint::getId, id));
                if (dupCount != null && dupCount > 0) {
                    throw new IllegalArgumentException("点位编码【" + newCode + "】已被其他点位占用，无法修改！");
                }
                point.setPointCode(newCode);
            }
        }
        if (dto.getDevicePassword() != null && !dto.getDevicePassword().trim().isEmpty()) {
            point.setDevicePasswordHash(passwordEncoder.encode(dto.getDevicePassword().trim()));
            point.setPointToken("ptk_" + UUID.randomUUID().toString().replace("-", ""));
        }
        if (dto.getAddress() != null) point.setAddress(dto.getAddress());
        if (dto.getCameraRtsp() != null) point.setCameraRtsp(dto.getCameraRtsp());
        if (dto.getResolution() != null) point.setResolution(dto.getResolution());
        if (dto.getBizType() != null && !dto.getBizType().trim().isEmpty()) point.setBizType(dto.getBizType().trim().toUpperCase());
        if (dto.getStreamStatus() != null) point.setStreamStatus(dto.getStreamStatus());

        pointMapper.updateById(point);
    }

    @Override
    @Transactional(rollbackFor = Exception.class)
    public void deletePointCascadeCloud(Long id) {
        BizPoint point = pointMapper.selectById(id);
        if (point == null) return;

        // 1. 删除云端数据库点位本身
        pointMapper.deleteById(id);

        // 2. 清理云端该点位下所有事件流水记录
        eventMapper.delete(new LambdaQueryWrapper<BizPassageEvent>().eq(BizPassageEvent::getPointId, id));

        // 3. 清理云端小时聚合数据
        statsMapper.delete(new LambdaQueryWrapper<BizStatsHourly>().eq(BizStatsHourly::getPointId, id));

        // 4. 清理设备心跳
        heartbeatMapper.deleteById(id);

        // 5. 异步/同步清理云端 MinIO 对象存储中的切片视频 (手机端彻底无法再看)
        // 注意：本地客户端(PC工控机)上的 SQLite 与 clips/ 磁盘文件完全自治保留，不被删除
        minioService.deletePointClips(id);
    }

    @Override
    public PointVO getPointById(Long id) {
        BizPoint point = pointMapper.selectById(id);
        return point != null ? toVO(point) : null;
    }

    @Override
    public BizPoint getPointByToken(String token) {
        if (token == null || token.trim().isEmpty()) return null;
        return pointMapper.selectOne(new LambdaQueryWrapper<BizPoint>()
                .eq(BizPoint::getPointToken, token.trim()));
    }

    @Override
    public String refreshToken(Long id) {
        BizPoint point = pointMapper.selectById(id);
        if (point == null) throw new IllegalArgumentException("点位不存在");
        String newToken = "ptk_" + UUID.randomUUID().toString().replace("-", "");
        point.setPointToken(newToken);
        pointMapper.updateById(point);
        return newToken;
    }

    @Override
    @Transactional(rollbackFor = Exception.class)
    public DeviceLoginVO loginWithDeviceCredentials(DeviceLoginDTO dto) {
        if (dto.getPointCode() == null || dto.getPointCode().trim().isEmpty()) {
            throw new IllegalArgumentException("点位编码不能为空");
        }
        if (dto.getDevicePassword() == null || dto.getDevicePassword().trim().isEmpty()) {
            throw new IllegalArgumentException("设备专属接入密码不能为空");
        }

        BizPoint point = pointMapper.selectOne(new LambdaQueryWrapper<BizPoint>()
                .eq(BizPoint::getPointCode, dto.getPointCode().trim()));
        if (point == null) {
            throw new IllegalArgumentException("未找到该编码对应的物理点位: " + dto.getPointCode());
        }

        String inputPwd = dto.getDevicePassword().trim();
        String storedHash = point.getDevicePasswordHash();

        boolean match = false;
        if (storedHash != null && !storedHash.isEmpty()) {
            match = passwordEncoder.matches(inputPwd, storedHash) || inputPwd.equals(storedHash);
        } else {
            // 若数据库尚未设置设备专属密码，默认初始化为当前输入并转为 BCrypt 散列
            match = true;
            point.setDevicePasswordHash(passwordEncoder.encode(inputPwd));
        }

        if (!match) {
            throw new IllegalArgumentException("边缘设备专属接入密码错误，请向商户管理员核对！");
        }

        // 若密码是明文匹配，自动升级为 BCrypt 哈希
        if (storedHash != null && !storedHash.startsWith("$2a$")) {
            point.setDevicePasswordHash(passwordEncoder.encode(inputPwd));
        }

        // 确保有合法的 pointToken
        if (point.getPointToken() == null || point.getPointToken().trim().isEmpty()) {
            point.setPointToken("ptk_" + UUID.randomUUID().toString().replace("-", ""));
        }
        point.setStreamStatus("ONLINE");
        pointMapper.updateById(point);

        return DeviceLoginVO.builder()
                .pointId(point.getId())
                .userId(point.getUserId())
                .pointCode(point.getPointCode())
                .pointName(point.getPointName())
                .pointToken(point.getPointToken())
                .cameraRtsp(point.getCameraRtsp() != null ? point.getCameraRtsp() : "")
                .resolution(point.getResolution() != null ? point.getResolution() : "1920x1080")
                .clientName(dto.getClientName() != null ? dto.getClientName() : "FlowSight-Node")
                .bizType(point.getBizType() != null ? point.getBizType() : "PEDESTRIAN")
                .build();
    }

    @Override
    public PointCredentialVO getPointCredential(Long pointId, Long userId) {
        BizPoint point = pointMapper.selectById(pointId);
        if (point == null) throw new IllegalArgumentException("点位不存在");
        if (userId != null && !userId.equals(point.getUserId())) {
            SysUser user = userMapper.selectById(userId);
            if (user == null || !"ROLE_ADMIN".equals(user.getRole())) {
                throw new IllegalArgumentException("无权查看该点位凭证");
            }
        }

        boolean hasPwd = point.getDevicePasswordHash() != null && !point.getDevicePasswordHash().trim().isEmpty();
        return PointCredentialVO.builder()
                .pointId(point.getId())
                .pointName(point.getPointName())
                .pointCode(point.getPointCode())
                .pointToken(point.getPointToken())
                .hasDevicePassword(hasPwd)
                .devicePasswordMasked(hasPwd ? "******" : "尚未设置")
                .build();
    }

    @Override
    @Transactional(rollbackFor = Exception.class)
    public PointCredentialVO resetDevicePassword(Long pointId, Long userId, ResetDevicePasswordDTO dto) {
        BizPoint point = pointMapper.selectById(pointId);
        if (point == null) throw new IllegalArgumentException("点位不存在");
        if (userId != null && !userId.equals(point.getUserId())) {
            SysUser user = userMapper.selectById(userId);
            if (user == null || !"ROLE_ADMIN".equals(user.getRole())) {
                throw new IllegalArgumentException("无权重置该点位设备密码");
            }
        }

        String newPwd = dto.getNewDevicePassword().trim();
        point.setDevicePasswordHash(passwordEncoder.encode(newPwd));
        // 重置设备密码的同时轮换 pointToken，立即切断旧有已泄露或失窃设备的连接
        String newToken = "ptk_" + UUID.randomUUID().toString().replace("-", "");
        point.setPointToken(newToken);
        pointMapper.updateById(point);

        return PointCredentialVO.builder()
                .pointId(point.getId())
                .pointName(point.getPointName())
                .pointCode(point.getPointCode())
                .pointToken(newToken)
                .hasDevicePassword(true)
                .devicePasswordMasked("******")
                .build();
    }

    private static class PairingSession {
        Long pointId;
        Long userId;
        long expireAt;

        PairingSession(Long pointId, Long userId, long expireAt) {
            this.pointId = pointId;
            this.userId = userId;
            this.expireAt = expireAt;
        }
    }

    private final Map<String, PairingSession> pairingCodeStore = new ConcurrentHashMap<>();

    @Override
    public PairingCodeVO generatePairingCode(Long pointId, Long userId) {
        BizPoint point = pointMapper.selectById(pointId);
        if (point == null) throw new IllegalArgumentException("点位不存在");
        if (userId != null && !userId.equals(point.getUserId())) {
            SysUser user = userMapper.selectById(userId);
            if (user == null || !"ROLE_ADMIN".equals(user.getRole())) {
                throw new IllegalArgumentException("无权为该点位生成配对码");
            }
        }

        // 生成 6 位随机数字码 (100000 - 999999)
        int randomCode = 100000 + new java.security.SecureRandom().nextInt(900000);
        String code = String.valueOf(randomCode);

        // 5 分钟 (300秒) 有效期
        long expireAt = System.currentTimeMillis() + 300 * 1000L;
        pairingCodeStore.put(code, new PairingSession(pointId, userId, expireAt));

        return PairingCodeVO.builder()
                .pairingCode(code)
                .pointId(point.getId())
                .pointName(point.getPointName())
                .expiresInSeconds(300)
                .build();
    }

    @Override
    @Transactional(rollbackFor = Exception.class)
    public DeviceLoginVO pairWithCode(PairingRequestDTO dto) {
        String code = dto.getPairingCode() != null ? dto.getPairingCode().trim().replace(" ", "") : "";
        if (code.length() != 6) {
            throw new IllegalArgumentException("配对码必须为 6 位数字");
        }

        PairingSession session = pairingCodeStore.get(code);
        if (session == null || System.currentTimeMillis() > session.expireAt) {
            pairingCodeStore.remove(code);
            throw new IllegalArgumentException("该 6 位配对码不存在或已过期，请在手机端重新生成！");
        }

        // 一次性消费配对码
        pairingCodeStore.remove(code);

        BizPoint point = pointMapper.selectById(session.pointId);
        if (point == null) {
            throw new IllegalArgumentException("配对码关联点位已不存在");
        }

        if (point.getPointToken() == null || point.getPointToken().trim().isEmpty()) {
            point.setPointToken("ptk_" + UUID.randomUUID().toString().replace("-", ""));
        }
        point.setStreamStatus("ONLINE");
        pointMapper.updateById(point);

        return DeviceLoginVO.builder()
                .pointId(point.getId())
                .userId(point.getUserId())
                .pointCode(point.getPointCode())
                .pointName(point.getPointName())
                .pointToken(point.getPointToken())
                .cameraRtsp(point.getCameraRtsp() != null ? point.getCameraRtsp() : "")
                .resolution(point.getResolution() != null ? point.getResolution() : "1920x1080")
                .clientName(dto.getClientName() != null ? dto.getClientName() : "FlowSight-Node")
                .bizType(point.getBizType() != null ? point.getBizType() : "PEDESTRIAN")
                .build();
    }

    private PointVO toVO(BizPoint p) {
        PointVO vo = new PointVO();
        vo.setId(p.getId());
        vo.setUserId(p.getUserId());
        vo.setPointName(p.getPointName());
        vo.setPointCode(p.getPointCode());
        vo.setAddress(p.getAddress());
        vo.setCameraRtsp(p.getCameraRtsp());
        vo.setResolution(p.getResolution());
        vo.setPointToken(p.getPointToken());
        vo.setBizType(p.getBizType() != null ? p.getBizType() : "PEDESTRIAN");
        vo.setCreateTime(p.getCreateTime());

        // 查询属主客户信息
        SysUser user = userMapper.selectById(p.getUserId());
        if (user != null) {
            vo.setUserPhone(user.getPhone());
            vo.setUserNickname(user.getNickname());
            vo.setCompanyName(user.getCompanyName());
        }

        // 查询实时心跳并实时判定在线状态 (超过 15 秒无心跳视为离线，防范时区负数偏差)
        BizDeviceHeartbeat hb = heartbeatMapper.selectById(p.getId());
        boolean isOnline = false;
        String boundClient = null;
        if (hb != null) {
            boundClient = hb.getClientName();
            if (hb.getLastHeartbeat() != null) {
                long secondsSinceLastHeartbeat = ChronoUnit.SECONDS.between(hb.getLastHeartbeat(), LocalDateTime.now());
                if (secondsSinceLastHeartbeat >= 0 && secondsSinceLastHeartbeat <= 15) {
                    isOnline = true;
                }
            }
        }
        String currentStatus = isOnline ? "ONLINE" : "OFFLINE";
        vo.setStreamStatus(currentStatus);
        vo.setIsOccupied(isOnline);
        vo.setBoundClientName(boundClient);

        if (!isOnline && "ONLINE".equalsIgnoreCase(p.getStreamStatus())) {
            p.setStreamStatus("OFFLINE");
            pointMapper.updateById(p);
        } else if (isOnline && !"ONLINE".equalsIgnoreCase(p.getStreamStatus())) {
            p.setStreamStatus("ONLINE");
            pointMapper.updateById(p);
        }

        if (hb != null) {
            vo.setCurrentFps(isOnline ? hb.getCurrentFps() : 0.0f);
            vo.setCurrentOccupancy(isOnline ? hb.getCurrentOccupancy() : 0);
            vo.setLastHeartbeat(hb.getLastHeartbeat());
        }
        return vo;
    }

    @Override
    public DeviceLoginVO bindPoint(Long pointId, String clientName, boolean force) {
        BizPoint point = pointMapper.selectById(pointId);
        if (point == null) {
            throw new IllegalArgumentException("点位不存在");
        }

        String targetClient = (clientName != null && !clientName.trim().isEmpty()) ? clientName.trim() : "FlowSight-Node";

        // 排他互斥校验：检查当前点位是否已被其他活跃设备独占
        BizDeviceHeartbeat hb = heartbeatMapper.selectById(pointId);
        if (!force && hb != null && hb.getLastHeartbeat() != null) {
            long seconds = ChronoUnit.SECONDS.between(hb.getLastHeartbeat(), LocalDateTime.now());
            if (seconds >= 0 && seconds <= 15) {
                String existingClient = hb.getClientName() != null ? hb.getClientName() : "未知设备";
                if (!targetClient.equalsIgnoreCase(existingClient)) {
                    throw new IllegalStateException("点位【" + point.getPointName() + "】当前已被设备【" + existingClient + "】独占在线，请勿重复绑定！");
                }
            }
        }

        // 确保有长效 Token
        if (point.getPointToken() == null || point.getPointToken().trim().isEmpty()) {
            point.setPointToken("ptk_" + UUID.randomUUID().toString().replace("-", ""));
        }
        point.setStreamStatus("ONLINE");
        pointMapper.updateById(point);

        boolean isNew = (hb == null);
        if (isNew) {
            hb = new BizDeviceHeartbeat();
            hb.setPointId(pointId);
            hb.setClientVersion("v1.2.0");
            hb.setHardwareBackend("CPU");
        }
        hb.setClientName(targetClient);
        hb.setCurrentFps(30.0f);
        hb.setCurrentOccupancy(0);
        hb.setLastHeartbeat(LocalDateTime.now());
        if (isNew) {
            heartbeatMapper.insert(hb);
        } else {
            heartbeatMapper.updateById(hb);
        }

        log.info("[PointService] 点位 ID: {}, 点位名称: '{}' 已成功被边缘设备 '{}' 独占锁定绑定 (force={})",
                pointId, point.getPointName(), targetClient, force);

        return DeviceLoginVO.builder()
                .pointId(point.getId())
                .userId(point.getUserId())
                .pointCode(point.getPointCode())
                .pointName(point.getPointName())
                .pointToken(point.getPointToken())
                .cameraRtsp(point.getCameraRtsp() != null ? point.getCameraRtsp() : "")
                .resolution(point.getResolution() != null ? point.getResolution() : "1920x1080")
                .clientName(targetClient)
                .bizType(point.getBizType() != null ? point.getBizType() : "PEDESTRIAN")
                .build();
    }

    @Override
    public void unbindPoint(Long pointId, String clientName) {
        BizPoint point = pointMapper.selectById(pointId);
        if (point == null) return;

        point.setStreamStatus("OFFLINE");
        pointMapper.updateById(point);

        BizDeviceHeartbeat hb = heartbeatMapper.selectById(pointId);
        if (hb != null) {
            hb.setCurrentOccupancy(0);
            hb.setCurrentFps(0.0f);
            hb.setLastHeartbeat(LocalDateTime.now().minusSeconds(60));
            heartbeatMapper.updateById(hb);
        }
        log.info("[PointService] 点位 ID: {}, 点位名称: '{}' 已被客户端 '{}' 主动解绑下线并释放独占锁",
                pointId, point.getPointName(), clientName);
    }

    @Override
    public String generateUniquePointCode(String bizType) {
        String typePart = "PED";
        if (bizType != null && !bizType.trim().isEmpty()) {
            String b = bizType.trim().toUpperCase();
            if ("VEHICLE".equalsIgnoreCase(b)) {
                typePart = "VEH";
            } else if ("PEDESTRIAN".equalsIgnoreCase(b)) {
                typePart = "PED";
            } else {
                typePart = b.length() > 4 ? b.substring(0, 4) : b;
            }
        }
        String prefix = "PT_" + typePart + "_";
        String dateStr = LocalDate.now().format(DateTimeFormatter.ofPattern("yyyyMMdd"));
        for (int i = 0; i < 30; i++) {
            String suffix = UUID.randomUUID().toString().replace("-", "").substring(0, 5).toUpperCase();
            String candidate = prefix + dateStr + "_" + suffix;
            Long count = pointMapper.selectCount(new LambdaQueryWrapper<BizPoint>()
                    .eq(BizPoint::getPointCode, candidate));
            if (count == null || count == 0) {
                return candidate;
            }
        }
        return prefix + dateStr + "_" + (System.currentTimeMillis() % 100000);
    }
}
