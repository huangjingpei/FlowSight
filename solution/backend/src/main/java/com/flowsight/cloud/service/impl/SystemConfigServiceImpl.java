package com.flowsight.cloud.service.impl;

import com.baomidou.mybatisplus.core.conditions.query.LambdaQueryWrapper;
import com.flowsight.cloud.entity.SysSystemConfig;
import com.flowsight.cloud.mapper.SysSystemConfigMapper;
import com.flowsight.cloud.service.SystemConfigService;
import com.flowsight.cloud.vo.SystemFeaturesVO;
import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.time.LocalDateTime;
import java.util.List;
import java.util.concurrent.TimeUnit;

@Slf4j
@Service
public class SystemConfigServiceImpl implements SystemConfigService {

    private static final String REDIS_CONFIG_KEY_PREFIX = "flowsight:sys:config:";
    private static final String KEY_VIDEO_PLAYBACK = "video_playback_enabled";

    @Autowired
    private SysSystemConfigMapper sysSystemConfigMapper;

    @Autowired(required = false)
    private StringRedisTemplate stringRedisTemplate;

    @Override
    public SystemFeaturesVO getSystemFeatures() {
        return SystemFeaturesVO.builder()
                .videoPlaybackEnabled(isVideoPlaybackEnabled())
                .build();
    }

    @Override
    public List<SysSystemConfig> listConfigs() {
        return sysSystemConfigMapper.selectList(new LambdaQueryWrapper<SysSystemConfig>()
                .orderByAsc(SysSystemConfig::getId));
    }

    @Override
    @Transactional(rollbackFor = Exception.class)
    public void updateConfig(String configKey, String configValue) {
        if (configKey == null || configKey.trim().isEmpty()) {
            throw new IllegalArgumentException("配置标识键名不能为空");
        }
        String key = configKey.trim();
        String val = configValue != null ? configValue.trim() : "";

        SysSystemConfig config = sysSystemConfigMapper.selectOne(new LambdaQueryWrapper<SysSystemConfig>()
                .eq(SysSystemConfig::getConfigKey, key));

        if (config == null) {
            config = new SysSystemConfig();
            config.setConfigKey(key);
            config.setConfigValue(val);
            config.setConfigName(key);
            config.setDescription("自定义系统配置项");
            config.setUpdateTime(LocalDateTime.now());
            sysSystemConfigMapper.insert(config);
        } else {
            config.setConfigValue(val);
            config.setUpdateTime(LocalDateTime.now());
            sysSystemConfigMapper.updateById(config);
        }

        // 同步更新 Redis 缓存
        if (stringRedisTemplate != null) {
            try {
                stringRedisTemplate.opsForValue().set(REDIS_CONFIG_KEY_PREFIX + key, val, 1, TimeUnit.HOURS);
            } catch (Exception ex) {
                log.warn("同步更新系统配置 Redis 缓存失败: {}", ex.getMessage());
            }
        }
    }

    @Override
    public boolean isVideoPlaybackEnabled() {
        // 1. 尝试从 Redis 读取
        if (stringRedisTemplate != null) {
            try {
                String cachedVal = stringRedisTemplate.opsForValue().get(REDIS_CONFIG_KEY_PREFIX + KEY_VIDEO_PLAYBACK);
                if (cachedVal != null) {
                    return "true".equalsIgnoreCase(cachedVal.trim()) || "1".equals(cachedVal.trim());
                }
            } catch (Exception ex) {
                log.warn("读取系统配置 Redis 缓存失败: {}", ex.getMessage());
            }
        }

        // 2. 从数据库读取
        SysSystemConfig config = sysSystemConfigMapper.selectOne(new LambdaQueryWrapper<SysSystemConfig>()
                .eq(SysSystemConfig::getConfigKey, KEY_VIDEO_PLAYBACK));

        boolean enabled = false;
        if (config != null && config.getConfigValue() != null) {
            String cv = config.getConfigValue().trim();
            enabled = "true".equalsIgnoreCase(cv) || "1".equals(cv);
        }

        // 3. 回写 Redis (默认 1 小时缓存)
        if (stringRedisTemplate != null) {
            try {
                stringRedisTemplate.opsForValue().set(
                        REDIS_CONFIG_KEY_PREFIX + KEY_VIDEO_PLAYBACK,
                        String.valueOf(enabled),
                        1,
                        TimeUnit.HOURS
                );
            } catch (Exception ex) {
                log.warn("写入系统配置 Redis 缓存失败: {}", ex.getMessage());
            }
        }

        return enabled;
    }
}
