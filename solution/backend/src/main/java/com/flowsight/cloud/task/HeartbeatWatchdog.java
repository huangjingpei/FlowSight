package com.flowsight.cloud.task;

import com.baomidou.mybatisplus.core.conditions.query.LambdaQueryWrapper;
import com.flowsight.cloud.entity.BizDeviceHeartbeat;
import com.flowsight.cloud.entity.BizPoint;
import com.flowsight.cloud.mapper.BizDeviceHeartbeatMapper;
import com.flowsight.cloud.mapper.BizPointMapper;
import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Component;

import java.time.LocalDateTime;
import java.time.temporal.ChronoUnit;
import java.util.List;
import java.util.Map;
import java.util.function.Function;
import java.util.stream.Collectors;

/**
 * 边缘设备在线状态看门狗服务
 * 定时主动巡检所有点位心跳，若超过 15 秒无上报，将点位自动切换为 OFFLINE
 */
@Slf4j
@Component
public class HeartbeatWatchdog {

    @Autowired
    private BizPointMapper pointMapper;

    @Autowired
    private BizDeviceHeartbeatMapper heartbeatMapper;

    @Scheduled(fixedRate = 5000)
    public void scanDeviceHeartbeats() {
        // 1. 查询当前所有在线的点位
        List<BizPoint> onlinePoints = pointMapper.selectList(new LambdaQueryWrapper<BizPoint>()
                .eq(BizPoint::getStreamStatus, "ONLINE"));

        if (onlinePoints == null || onlinePoints.isEmpty()) {
            return;
        }

        // 2. 批量查询对应心跳记录，消除循环单条查询的 N+1 数据库性能隐患
        List<Long> pointIds = onlinePoints.stream().map(BizPoint::getId).collect(Collectors.toList());
        List<BizDeviceHeartbeat> heartbeats = heartbeatMapper.selectBatchIds(pointIds);
        Map<Long, BizDeviceHeartbeat> hbMap = (heartbeats != null) ?
                heartbeats.stream().collect(Collectors.toMap(BizDeviceHeartbeat::getPointId, Function.identity(), (a, b) -> a))
                : Map.of();

        LocalDateTime now = LocalDateTime.now();
        for (BizPoint point : onlinePoints) {
            BizDeviceHeartbeat hb = hbMap.get(point.getId());
            boolean isTimeout = false;
            if (hb == null || hb.getLastHeartbeat() == null) {
                isTimeout = true;
            } else {
                long elapsedSeconds = ChronoUnit.SECONDS.between(hb.getLastHeartbeat(), now);
                if (elapsedSeconds < 0 || elapsedSeconds > 15) {
                    isTimeout = true;
                }
            }

            if (isTimeout) {
                point.setStreamStatus("OFFLINE");
                pointMapper.updateById(point);
                if (hb != null) {
                    hb.setCurrentOccupancy(0);
                    hb.setCurrentFps(0.0f);
                    heartbeatMapper.updateById(hb);
                }
                log.info("[HeartbeatWatchdog] 点位 ID: {}, 点位名称: '{}' 超过 15 秒无心跳，状态已切换为: OFFLINE",
                        point.getId(), point.getPointName());
            }
        }
    }
}
