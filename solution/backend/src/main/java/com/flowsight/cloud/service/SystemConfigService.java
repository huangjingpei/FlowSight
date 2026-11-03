package com.flowsight.cloud.service;

import com.flowsight.cloud.entity.SysSystemConfig;
import com.flowsight.cloud.vo.SystemFeaturesVO;

import java.util.List;

public interface SystemConfigService {
    /**
     * 获取公开系统特性与开关状态 (供 H5/Web 等前端自适应展示)
     */
    SystemFeaturesVO getSystemFeatures();

    /**
     * 获取全部配置项列表 (供管理员后台查看)
     */
    List<SysSystemConfig> listConfigs();

    /**
     * 更新指定配置项
     */
    void updateConfig(String configKey, String configValue);

    /**
     * 判断视频播放与证据切片功能是否开启
     */
    boolean isVideoPlaybackEnabled();
}
