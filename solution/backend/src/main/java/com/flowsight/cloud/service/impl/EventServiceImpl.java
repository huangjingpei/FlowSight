package com.flowsight.cloud.service.impl;

import com.baomidou.mybatisplus.core.conditions.query.LambdaQueryWrapper;
import com.baomidou.mybatisplus.extension.plugins.pagination.Page;
import com.flowsight.cloud.dto.EdgeEventDTO;
import com.flowsight.cloud.dto.EdgeHeartbeatDTO;
import com.flowsight.cloud.entity.BizDeviceHeartbeat;
import com.flowsight.cloud.entity.BizPassageEvent;
import com.flowsight.cloud.entity.BizPoint;
import com.flowsight.cloud.entity.BizStatsHourly;
import com.flowsight.cloud.mapper.BizDeviceHeartbeatMapper;
import com.flowsight.cloud.mapper.BizPassageEventMapper;
import com.flowsight.cloud.mapper.BizPointMapper;
import com.flowsight.cloud.mapper.BizStatsHourlyMapper;
import com.flowsight.cloud.service.EventService;
import com.flowsight.cloud.service.MinioService;
import com.flowsight.cloud.vo.DashboardSummaryVO;
import com.flowsight.cloud.vo.DemographicsVO;
import com.flowsight.cloud.vo.EventItemVO;
import com.flowsight.cloud.vo.HourlyStatVO;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;
import org.springframework.web.multipart.MultipartFile;

import java.time.Instant;
import java.time.LocalDate;
import java.time.LocalDateTime;
import java.time.ZoneId;
import java.time.format.DateTimeFormatter;
import java.util.*;
import java.util.stream.Collectors;

@Service
public class EventServiceImpl implements EventService {

    @Autowired
    private BizPassageEventMapper eventMapper;

    @Autowired
    private BizStatsHourlyMapper statsMapper;

    @Autowired
    private BizPointMapper pointMapper;

    @Autowired
    private BizDeviceHeartbeatMapper heartbeatMapper;

    @Autowired
    private MinioService minioService;

    @Override
    @Transactional(rollbackFor = Exception.class)
    public void recordEdgeEvent(Long pointId, EdgeEventDTO dto) {
        BizPoint point = pointMapper.selectById(pointId);
        if (point == null) {
            throw new IllegalArgumentException("点位不存在: " + pointId);
        }

        long ts = dto.getTimestamp() != null ? dto.getTimestamp() : System.currentTimeMillis();
        String gender = "未知";
        String ageGroup = "未知";
        if (dto.getAttributes() != null) {
            gender = dto.getAttributes().getOrDefault("gender", "未知");
            ageGroup = dto.getAttributes().getOrDefault("age_group", "未知");
        }

        BizPassageEvent event = new BizPassageEvent();
        event.setPointId(pointId);
        event.setUserId(point.getUserId());
        event.setTrackId(dto.getTrackId() != null ? dto.getTrackId() : 0);
        event.setDirection(dto.getDirection() != null ? dto.getDirection() : "ENTRY");
        event.setConfidence(dto.getConfidence() != null ? dto.getConfidence() : 0.85f);
        event.setGender(gender);
        event.setAgeGroup(ageGroup);
        event.setTimestampMs(ts);
        event.setLocalClipPath(dto.getClipPath() != null ? dto.getClipPath() : "");

        eventMapper.insert(event);

        // 更新小时预聚合统计 (stats_hourly)
        updateHourlyStat(pointId, point.getUserId(), ts, event.getDirection(), gender);
    }

    private void updateHourlyStat(Long pointId, Long userId, long ts, String direction, String gender) {
        LocalDateTime ldt = LocalDateTime.ofInstant(Instant.ofEpochMilli(ts), ZoneId.systemDefault());
        LocalDate date = ldt.toLocalDate();
        int hour = ldt.getHour();

        BizStatsHourly stat = statsMapper.selectOne(new LambdaQueryWrapper<BizStatsHourly>()
                .eq(BizStatsHourly::getPointId, pointId)
                .eq(BizStatsHourly::getStatDate, date)
                .eq(BizStatsHourly::getStatHour, hour));

        boolean isEntry = "ENTRY".equalsIgnoreCase(direction);
        boolean isMale = "男".equals(gender);
        boolean isFemale = "女".equals(gender);

        if (stat == null) {
            stat = new BizStatsHourly();
            stat.setPointId(pointId);
            stat.setUserId(userId);
            stat.setStatDate(date);
            stat.setStatHour(hour);
            stat.setEntryCount(isEntry ? 1 : 0);
            stat.setExitCount(isEntry ? 0 : 1);
            stat.setMaleCount(isMale ? 1 : 0);
            stat.setFemaleCount(isFemale ? 1 : 0);
            statsMapper.insert(stat);
        } else {
            if (isEntry) stat.setEntryCount(stat.getEntryCount() + 1);
            else stat.setExitCount(stat.getExitCount() + 1);
            if (isMale) stat.setMaleCount(stat.getMaleCount() + 1);
            if (isFemale) stat.setFemaleCount(stat.getFemaleCount() + 1);
            statsMapper.updateById(stat);
        }
    }

    @Override
    public String uploadEdgeClip(Long pointId, Integer trackId, Long timestamp, MultipartFile file) {
        String clipUrl = minioService.uploadClip(pointId, trackId, timestamp, file);

        // 回填到最新的事件记录中 (如果已入库)
        if (trackId != null && timestamp != null) {
            List<BizPassageEvent> list = eventMapper.selectList(new LambdaQueryWrapper<BizPassageEvent>()
                    .eq(BizPassageEvent::getPointId, pointId)
                    .eq(BizPassageEvent::getTrackId, trackId)
                    .ge(BizPassageEvent::getTimestampMs, timestamp - 10000)
                    .le(BizPassageEvent::getTimestampMs, timestamp + 10000)
                    .orderByDesc(BizPassageEvent::getId));
            if (!list.isEmpty()) {
                BizPassageEvent ev = list.get(0);
                ev.setClipUrl(clipUrl);
                eventMapper.updateById(ev);
            }
        }
        return clipUrl;
    }

    @Override
    public void recordHeartbeat(Long pointId, EdgeHeartbeatDTO dto) {
        BizPoint point = pointMapper.selectById(pointId);
        if (point == null) return;

        BizDeviceHeartbeat hb = heartbeatMapper.selectById(pointId);
        boolean isNew = (hb == null);
        if (isNew) {
            hb = new BizDeviceHeartbeat();
            hb.setPointId(pointId);
        }
        hb.setClientVersion(dto.getClientVersion() != null ? dto.getClientVersion() : "v1.2.0");
        hb.setCurrentFps(dto.getFps() != null ? dto.getFps() : 0.0f);
        hb.setInferLatencyMs(dto.getInferLatencyMs() != null ? dto.getInferLatencyMs() : 0.0f);
        hb.setHardwareBackend(dto.getHardwareBackend() != null ? dto.getHardwareBackend() : "CPU");
        hb.setCurrentOccupancy(dto.getCurrentOccupancy() != null ? dto.getCurrentOccupancy() : 0);
        hb.setLastHeartbeat(LocalDateTime.now());

        if (isNew) {
            heartbeatMapper.insert(hb);
        } else {
            heartbeatMapper.updateById(hb);
        }

        // 更新点位状态为 ONLINE
        point.setStreamStatus("ONLINE");
        pointMapper.updateById(point);
    }

    @Override
    public DashboardSummaryVO getDashboardSummary(Long pointId) {
        BizPoint point = pointMapper.selectById(pointId);
        if (point == null) {
            throw new IllegalArgumentException("点位不存在");
        }

        LocalDate today = LocalDate.now();
        List<BizStatsHourly> hourlyList = statsMapper.selectList(new LambdaQueryWrapper<BizStatsHourly>()
                .eq(BizStatsHourly::getPointId, pointId)
                .eq(BizStatsHourly::getStatDate, today));

        int todayIn = 0;
        int todayOut = 0;
        int totalMale = 0;
        int totalFemale = 0;

        Map<Integer, HourlyStatVO> hourMap = new HashMap<>();
        for (int i = 0; i < 24; i++) {
            HourlyStatVO vo = new HourlyStatVO();
            vo.setHour(i);
            vo.setEntry(0);
            vo.setExit(0);
            hourMap.put(i, vo);
        }

        for (BizStatsHourly stat : hourlyList) {
            todayIn += stat.getEntryCount();
            todayOut += stat.getExitCount();
            totalMale += stat.getMaleCount();
            totalFemale += stat.getFemaleCount();

            HourlyStatVO hvo = hourMap.get(stat.getStatHour());
            if (hvo != null) {
                hvo.setEntry(stat.getEntryCount());
                hvo.setExit(stat.getExitCount());
            }
        }

        // 客群画像比例
        DemographicsVO demo = new DemographicsVO();
        demo.setMaleCount(totalMale);
        demo.setFemaleCount(totalFemale);
        int totalGender = totalMale + totalFemale;
        demo.setMaleRatio(totalGender > 0 ? (double) totalMale / totalGender : 0.5);
        demo.setFemaleRatio(totalGender > 0 ? (double) totalFemale / totalGender : 0.5);
        demo.setTopAgeGroup("青年(18-35)");

        // 查找最主要年龄段
        List<BizPassageEvent> todayEvents = eventMapper.selectList(new LambdaQueryWrapper<BizPassageEvent>()
                .eq(BizPassageEvent::getPointId, pointId)
                .ge(BizPassageEvent::getTimestampMs, today.atStartOfDay(ZoneId.systemDefault()).toInstant().toEpochMilli())
                .select(BizPassageEvent::getAgeGroup));
        if (!todayEvents.isEmpty()) {
            Map<String, Long> ageCount = todayEvents.stream()
                    .filter(e -> e.getAgeGroup() != null && !"未知".equals(e.getAgeGroup()))
                    .collect(Collectors.groupingBy(BizPassageEvent::getAgeGroup, Collectors.counting()));
            ageCount.entrySet().stream()
                    .max(Map.Entry.comparingByValue())
                    .ifPresent(entry -> demo.setTopAgeGroup(entry.getKey()));
        }

        // 获取最新心跳滞留
        BizDeviceHeartbeat hb = heartbeatMapper.selectById(pointId);
        int occupancy = hb != null ? hb.getCurrentOccupancy() : 0;

        DashboardSummaryVO vo = new DashboardSummaryVO();
        vo.setPointId(point.getId());
        vo.setPointName(point.getPointName());
        vo.setStreamStatus(point.getStreamStatus());
        vo.setCurrentOccupancy(occupancy);
        vo.setTodayEntry(todayIn);
        vo.setTodayExit(todayOut);
        vo.setTodayTotal(todayIn + todayOut);
        vo.setDemographics(demo);
        vo.setHourlyChart(new ArrayList<>(hourMap.values()));
        vo.getHourlyChart().sort(Comparator.comparingInt(HourlyStatVO::getHour));

        return vo;
    }

    @Override
    public Page<EventItemVO> pageEvents(Long pointId, int page, int size) {
        Page<BizPassageEvent> eventPage = new Page<>(page, size);
        LambdaQueryWrapper<BizPassageEvent> wrapper = new LambdaQueryWrapper<BizPassageEvent>()
                .eq(BizPassageEvent::getPointId, pointId)
                .orderByDesc(BizPassageEvent::getTimestampMs);

        Page<BizPassageEvent> res = eventMapper.selectPage(eventPage, wrapper);
        Page<EventItemVO> voPage = new Page<>(res.getCurrent(), res.getSize(), res.getTotal());

        voPage.setRecords(res.getRecords().stream().map(e -> {
            EventItemVO vo = new EventItemVO();
            vo.setId(e.getId());
            vo.setPointId(e.getPointId());
            vo.setTrackId(e.getTrackId());
            vo.setDirection(e.getDirection());
            vo.setConfidence(e.getConfidence());
            vo.setGender(e.getGender());
            vo.setAgeGroup(e.getAgeGroup());
            vo.setTimestampMs(e.getTimestampMs());

            LocalDateTime ldt = LocalDateTime.ofInstant(Instant.ofEpochMilli(e.getTimestampMs()), ZoneId.systemDefault());
            vo.setDatetime(ldt.format(DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss")));

            // 生成带签名的可播放直链
            vo.setPlayUrl(minioService.getPresignedPlayUrl(e.getClipUrl()));
            return vo;
        }).collect(Collectors.toList()));

        return voPage;
    }
}
