package com.flowsight.cloud.service.impl;

import com.baomidou.mybatisplus.core.conditions.query.LambdaQueryWrapper;
import com.baomidou.mybatisplus.extension.plugins.pagination.Page;
import com.flowsight.cloud.dto.EdgeBatchReportDTO;
import com.flowsight.cloud.dto.EdgeEventDTO;
import com.flowsight.cloud.dto.EdgeHeartbeatDTO;
import com.flowsight.cloud.entity.BizDeviceHeartbeat;
import com.flowsight.cloud.entity.BizPassageEvent;
import com.flowsight.cloud.entity.BizPoint;
import com.flowsight.cloud.entity.BizStatsHourly;
import com.flowsight.cloud.entity.SysUser;
import com.flowsight.cloud.mapper.BizDeviceHeartbeatMapper;
import com.flowsight.cloud.mapper.BizPassageEventMapper;
import com.flowsight.cloud.mapper.BizPointMapper;
import com.flowsight.cloud.mapper.BizStatsHourlyMapper;
import com.flowsight.cloud.mapper.SysUserMapper;
import com.flowsight.cloud.service.EventService;
import com.flowsight.cloud.service.MinioService;
import com.flowsight.cloud.vo.DashboardSummaryVO;
import com.flowsight.cloud.vo.DemographicsVO;
import com.flowsight.cloud.vo.EventItemVO;
import com.flowsight.cloud.vo.HourlyStatVO;
import com.flowsight.cloud.vo.TrendReportVO;
import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;
import org.springframework.web.multipart.MultipartFile;

import java.io.File;
import java.time.Instant;
import java.time.LocalDate;
import java.time.LocalDateTime;
import java.time.ZoneId;
import java.time.format.DateTimeFormatter;
import java.time.temporal.ChronoUnit;
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;
import java.util.stream.Collectors;

@Slf4j
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
    private SysUserMapper userMapper;

    @Autowired
    private MinioService minioService;

    // 待绑定切片池: 解决异步上传中切片先于聚合指标或后于聚合指标到达的时序差问题
    private final Map<String, String> pendingClipUrlCache = new ConcurrentHashMap<>();

    // 批次 UUID 幂等缓存 (用于防网络重试、离线队列重复重放导致指标双计)
    private final Map<String, Long> processedBatchUuids = new ConcurrentHashMap<>();
    private static final int MAX_BATCH_CACHE_SIZE = 10000;
    private static final long BATCH_CACHE_EXPIRE_MS = 24 * 3600 * 1000L; // 24小时

    private void cleanBatchCacheIfNeeded() {
        if (processedBatchUuids.size() > MAX_BATCH_CACHE_SIZE) {
            long now = System.currentTimeMillis();
            processedBatchUuids.entrySet().removeIf(entry -> (now - entry.getValue()) > BATCH_CACHE_EXPIRE_MS);
        }
    }

    @Override
    @Transactional(rollbackFor = Exception.class)
    public void recordEdgeEvent(Long pointId, EdgeEventDTO dto) {
        BizPoint point = pointMapper.selectById(pointId);
        if (point == null) {
            throw new IllegalArgumentException("点位不存在: " + pointId);
        }

        long ts = dto.getTimestamp() != null ? dto.getTimestamp() : System.currentTimeMillis();

        // 商业级端云防重保障：检查同点位、同TrackId在 15 秒内是否已有记录
        if (dto.getTrackId() != null && dto.getTrackId() > 0) {
            List<BizPassageEvent> existing = eventMapper.selectList(new LambdaQueryWrapper<BizPassageEvent>()
                    .eq(BizPassageEvent::getPointId, pointId)
                    .eq(BizPassageEvent::getTrackId, dto.getTrackId())
                    .ge(BizPassageEvent::getTimestampMs, ts - 15000)
                    .le(BizPassageEvent::getTimestampMs, ts + 15000));
            if (!existing.isEmpty()) {
                log.info("单事件端云防重拦截：点位 {} TrackId {} 在时间戳 {} 附近已存在记录，跳过入库与计数", pointId, dto.getTrackId(), ts);
                return;
            }
        }

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
        event.setPlateNumber(dto.getPlateNumber() != null ? dto.getPlateNumber() : "");
        event.setPlateColor(dto.getPlateColor() != null ? dto.getPlateColor() : "");
        event.setVehicleType(dto.getVehicleType() != null ? dto.getVehicleType() : "");
        event.setVehicleColor(dto.getVehicleColor() != null ? dto.getVehicleColor() : "");
        event.setTimestampMs(ts);
        event.setLocalClipPath(dto.getClipPath() != null ? dto.getClipPath() : "");

        eventMapper.insert(event);

        // 更新小时预聚合统计 (stats_hourly)
        updateHourlyStat(pointId, point.getUserId(), ts, event.getDirection(), gender);
    }

    @Override
    @Transactional(rollbackFor = Exception.class)
    public void recordEdgeBatchReport(Long pointId, EdgeBatchReportDTO dto) {
        BizPoint point = pointMapper.selectById(pointId);
        if (point == null) {
            throw new IllegalArgumentException("点位不存在: " + pointId);
        }

        boolean isDuplicateBatch = false;
        if (dto.getBatchUuid() != null && !dto.getBatchUuid().trim().isEmpty()) {
            cleanBatchCacheIfNeeded();
            Long previousTs = processedBatchUuids.putIfAbsent(dto.getBatchUuid().trim(), System.currentTimeMillis());
            if (previousTs != null) {
                isDuplicateBatch = true;
                log.warn("批次 UUID [{}] 已经成功入库，触发幂等保护，跳过指标累加与明细入库", dto.getBatchUuid());
            }
        }

        if (!isDuplicateBatch) {
            long endTs = dto.getEndTimeMs() != null ? dto.getEndTimeMs() : System.currentTimeMillis();
            LocalDateTime ldt = LocalDateTime.ofInstant(Instant.ofEpochMilli(endTs), ZoneId.systemDefault());
            LocalDate date = ldt.toLocalDate();
            int hour = ldt.getHour();

            // 1. 原子累加小时聚合表指标
            int inInc = (dto.getHeadcount() != null && dto.getHeadcount().getInIncrement() != null) ? dto.getHeadcount().getInIncrement() : 0;
            int outInc = (dto.getHeadcount() != null && dto.getHeadcount().getOutIncrement() != null) ? dto.getHeadcount().getOutIncrement() : 0;
            int maleInc = (dto.getDemographics() != null && dto.getDemographics().getMaleCount() != null) ? dto.getDemographics().getMaleCount() : 0;
            int femaleInc = (dto.getDemographics() != null && dto.getDemographics().getFemaleCount() != null) ? dto.getDemographics().getFemaleCount() : 0;

            BizStatsHourly stat = statsMapper.selectOne(new LambdaQueryWrapper<BizStatsHourly>()
                    .eq(BizStatsHourly::getPointId, pointId)
                    .eq(BizStatsHourly::getStatDate, date)
                    .eq(BizStatsHourly::getStatHour, hour));

            if (stat == null) {
                stat = new BizStatsHourly();
                stat.setPointId(pointId);
                stat.setUserId(point.getUserId());
                stat.setStatDate(date);
                stat.setStatHour(hour);
                stat.setEntryCount(inInc);
                stat.setExitCount(outInc);
                stat.setMaleCount(maleInc);
                stat.setFemaleCount(femaleInc);
                statsMapper.insert(stat);
            } else {
                stat.setEntryCount(stat.getEntryCount() + inInc);
                stat.setExitCount(stat.getExitCount() + outInc);
                stat.setMaleCount(stat.getMaleCount() + maleInc);
                stat.setFemaleCount(stat.getFemaleCount() + femaleInc);
                statsMapper.updateById(stat);
            }

            // 2. 批量存储精简事件明细 (如果客户端附带了 events)
            if (dto.getEvents() != null && !dto.getEvents().isEmpty()) {
                for (EdgeEventDTO evDto : dto.getEvents()) {
                    long evTs = evDto.getTimestamp() != null ? evDto.getTimestamp() : endTs;

                    // 商业级端云防重保障：检查同点位、同TrackId在 15 秒内是否已有记录
                    if (evDto.getTrackId() != null && evDto.getTrackId() > 0) {
                        List<BizPassageEvent> existing = eventMapper.selectList(new LambdaQueryWrapper<BizPassageEvent>()
                                .eq(BizPassageEvent::getPointId, pointId)
                                .eq(BizPassageEvent::getTrackId, evDto.getTrackId())
                                .ge(BizPassageEvent::getTimestampMs, evTs - 15000)
                                .le(BizPassageEvent::getTimestampMs, evTs + 15000));
                        if (!existing.isEmpty()) {
                            // 若切片先行到达，回填切片 URL 到已有记录
                            String pendingUrl = pendingClipUrlCache.remove(pointId + "_" + evDto.getTrackId());
                            BizPassageEvent existEv = existing.get(0);
                            boolean needUpdate = false;
                            if (pendingUrl != null && (existEv.getClipUrl() == null || existEv.getClipUrl().isEmpty())) {
                                existEv.setClipUrl(pendingUrl);
                                needUpdate = true;
                            }
                            if ((existEv.getPlateNumber() == null || existEv.getPlateNumber().isEmpty()) &&
                                evDto.getPlateNumber() != null && !evDto.getPlateNumber().isEmpty()) {
                                existEv.setPlateNumber(evDto.getPlateNumber());
                                existEv.setPlateColor(evDto.getPlateColor() != null ? evDto.getPlateColor() : "");
                                existEv.setVehicleType(evDto.getVehicleType() != null ? evDto.getVehicleType() : "");
                                existEv.setVehicleColor(evDto.getVehicleColor() != null ? evDto.getVehicleColor() : "");
                                needUpdate = true;
                            }
                            if (needUpdate) {
                                eventMapper.updateById(existEv);
                            }
                            log.info("点位 {} TrackId {} 在时间戳 {} 附近已存在通行记录，执行端云去重跳过重复插入", pointId, evDto.getTrackId(), evTs);
                            continue;
                        }
                    }

                    String gender = "未知";
                    String ageGroup = "未知";
                    if (evDto.getAttributes() != null) {
                        gender = evDto.getAttributes().getOrDefault("gender", "未知");
                        ageGroup = evDto.getAttributes().getOrDefault("age_group", "未知");
                    }

                    BizPassageEvent event = new BizPassageEvent();
                    event.setPointId(pointId);
                    event.setUserId(point.getUserId());
                    event.setTrackId(evDto.getTrackId() != null ? evDto.getTrackId() : 0);
                    event.setDirection(evDto.getDirection() != null ? evDto.getDirection() : "ENTRY");
                    event.setConfidence(evDto.getConfidence() != null ? evDto.getConfidence() : 0.85f);
                    event.setGender(gender);
                    event.setAgeGroup(ageGroup);
                    event.setPlateNumber(evDto.getPlateNumber() != null ? evDto.getPlateNumber() : "");
                    event.setPlateColor(evDto.getPlateColor() != null ? evDto.getPlateColor() : "");
                    event.setVehicleType(evDto.getVehicleType() != null ? evDto.getVehicleType() : "");
                    event.setVehicleColor(evDto.getVehicleColor() != null ? evDto.getVehicleColor() : "");
                    event.setTimestampMs(evTs);
                    event.setLocalClipPath(evDto.getClipPath() != null ? evDto.getClipPath() : "");

                    // 检查待绑定切片池 (若切片先行上传到达)
                    String pendingUrl = pendingClipUrlCache.remove(pointId + "_" + event.getTrackId());
                    if (pendingUrl != null) {
                        event.setClipUrl(pendingUrl);
                    }

                    eventMapper.insert(event);
                }
            }
        }

        // 3. 更新设备心跳与健康遥测状态
        BizDeviceHeartbeat hb = heartbeatMapper.selectById(pointId);
        boolean isNew = (hb == null);
        if (isNew) {
            hb = new BizDeviceHeartbeat();
            hb.setPointId(pointId);
        }
        if (dto.getClientName() != null && !dto.getClientName().trim().isEmpty()) {
            hb.setClientName(dto.getClientName());
        }
        if (dto.getAppVersion() != null && !dto.getAppVersion().trim().isEmpty()) {
            hb.setClientVersion(dto.getAppVersion());
        }
        if (dto.getTelemetry() != null) {
            hb.setCurrentFps(dto.getTelemetry().getFps() != null ? dto.getTelemetry().getFps() : 0.0f);
            hb.setInferLatencyMs(dto.getTelemetry().getInferLatencyMs() != null ? dto.getTelemetry().getInferLatencyMs() : 0.0f);
            hb.setHardwareBackend(dto.getTelemetry().getHardwareBackend() != null ? dto.getTelemetry().getHardwareBackend() : "CPU");
        }
        if (dto.getHeadcount() != null && dto.getHeadcount().getCurrentOccupancy() != null) {
            hb.setCurrentOccupancy(dto.getHeadcount().getCurrentOccupancy());
        }
        hb.setLastHeartbeat(LocalDateTime.now());

        if (isNew) {
            heartbeatMapper.insert(hb);
        } else {
            heartbeatMapper.updateById(hb);
        }

        // 点位状态在线
        point.setStreamStatus("ONLINE");
        pointMapper.updateById(point);
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
        boolean matched = false;
        if (trackId != null && timestamp != null) {
            List<BizPassageEvent> list = eventMapper.selectList(new LambdaQueryWrapper<BizPassageEvent>()
                    .eq(BizPassageEvent::getPointId, pointId)
                    .eq(BizPassageEvent::getTrackId, trackId)
                    .ge(BizPassageEvent::getTimestampMs, timestamp - 60000)
                    .le(BizPassageEvent::getTimestampMs, timestamp + 60000)
                    .orderByDesc(BizPassageEvent::getId));
            if (!list.isEmpty()) {
                BizPassageEvent ev = list.get(0);
                ev.setClipUrl(clipUrl);
                eventMapper.updateById(ev);
                matched = true;
            }
        }

        // 若事件记录尚未批量上报到达，暂存至待绑定池
        if (!matched && trackId != null) {
            pendingClipUrlCache.put(pointId + "_" + trackId, clipUrl);
        }

        return clipUrl;
    }

    @Override
    public String uploadEdgeSnapshot(Long pointId, MultipartFile file) {
        BizPoint point = pointMapper.selectById(pointId);
        if (point == null) throw new IllegalArgumentException("点位不存在: " + pointId);

        try {
            File dir = new File("uploads/snapshots").getAbsoluteFile();
            if (!dir.exists()) dir.mkdirs();

            String filename = "snapshot_p" + pointId + ".jpg";
            File dest = new File(dir, filename).getAbsoluteFile();
            file.transferTo(dest);

            String url = "/api/v1/snapshots/" + filename + "?t=" + System.currentTimeMillis();
            point.setSnapshotUrl(url);
            pointMapper.updateById(point);
            return url;
        } catch (Exception e) {
            throw new RuntimeException("快照保存失败: " + e.getMessage(), e);
        }
    }

    @Override
    public void recordHeartbeat(Long pointId, EdgeHeartbeatDTO dto) {
        BizPoint point = pointMapper.selectById(pointId);
        if (point != null) {
            recordHeartbeat(point, dto);
        }
    }

    @Override
    public void recordHeartbeat(BizPoint point, EdgeHeartbeatDTO dto) {
        if (point == null) return;
        Long pointId = point.getId();

        BizDeviceHeartbeat hb = heartbeatMapper.selectById(pointId);
        boolean isNew = (hb == null);
        if (isNew) {
            hb = new BizDeviceHeartbeat();
            hb.setPointId(pointId);
        }
        hb.setClientVersion(dto.getClientVersion() != null ? dto.getClientVersion() : "v1.2.0");
        if (dto.getClientName() != null && !dto.getClientName().trim().isEmpty()) {
            hb.setClientName(dto.getClientName().trim());
        }
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

        // 根据状态标志判断是否下线或上线 (状态无变化时严禁重复触发 updateById，保护数据库 WAL 与 I/O)
        if ("OFFLINE".equalsIgnoreCase(dto.getStatus())) {
            if (!"OFFLINE".equalsIgnoreCase(point.getStreamStatus())) {
                point.setStreamStatus("OFFLINE");
                pointMapper.updateById(point);
            }
            hb.setCurrentOccupancy(0);
            hb.setCurrentFps(0.0f);
            heartbeatMapper.updateById(hb);
        } else {
            if (!"ONLINE".equalsIgnoreCase(point.getStreamStatus())) {
                point.setStreamStatus("ONLINE");
                pointMapper.updateById(point);
            }
        }
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

        // 获取最新心跳滞留并进行实时在线超时判断 (若超 15s 未上报则判定 OFFLINE，防负数时钟漂移)
        BizDeviceHeartbeat hb = heartbeatMapper.selectById(pointId);
        boolean isOnline = false;
        if (hb != null && hb.getLastHeartbeat() != null) {
            long secondsSinceLastHeartbeat = ChronoUnit.SECONDS.between(hb.getLastHeartbeat(), LocalDateTime.now());
            if (secondsSinceLastHeartbeat >= 0 && secondsSinceLastHeartbeat <= 15) {
                isOnline = true;
            }
        }

        String currentStatus = isOnline ? "ONLINE" : "OFFLINE";
        // 同步校准数据库状态
        if (!isOnline && "ONLINE".equalsIgnoreCase(point.getStreamStatus())) {
            point.setStreamStatus("OFFLINE");
            pointMapper.updateById(point);
        } else if (isOnline && !"ONLINE".equalsIgnoreCase(point.getStreamStatus())) {
            point.setStreamStatus("ONLINE");
            pointMapper.updateById(point);
        }

        int occupancy = (isOnline && hb != null) ? hb.getCurrentOccupancy() : 0;

        DashboardSummaryVO vo = new DashboardSummaryVO();
        vo.setPointId(point.getId());
        vo.setPointName(point.getPointName());
        vo.setBizType(point.getBizType() != null ? point.getBizType() : "PEDESTRIAN");
        vo.setClientName(hb != null && hb.getClientName() != null ? hb.getClientName() : ("FlowSight-Node-" + point.getId()));
        vo.setAppVersion(hb != null && hb.getClientVersion() != null ? hb.getClientVersion() : "v1.2.0");
        if (hb != null && hb.getLastHeartbeat() != null) {
            vo.setLastHeartbeat(hb.getLastHeartbeat().format(DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss")));
        } else {
            vo.setLastHeartbeat("暂无上报记录");
        }
        vo.setStreamStatus(currentStatus);
        vo.setCurrentOccupancy(occupancy);
        vo.setTodayEntry(todayIn);
        vo.setTodayExit(todayOut);
        vo.setTodayTotal(todayIn + todayOut);
        vo.setSnapshotUrl(point.getSnapshotUrl() != null ? point.getSnapshotUrl() : "");
        vo.setDemographics(demo);
        vo.setHourlyChart(new ArrayList<>(hourMap.values()));
        vo.getHourlyChart().sort(Comparator.comparingInt(HourlyStatVO::getHour));

        return vo;
    }

    @Override
    public TrendReportVO getTrendReport(Long pointId, String dimension, Integer days) {
        if (dimension == null || dimension.trim().isEmpty()) {
            dimension = "realtime";
        }
        BizPoint point = pointMapper.selectById(pointId);
        boolean isVehMode = point != null && "VEHICLE".equalsIgnoreCase(point.getBizType());

        TrendReportVO vo = new TrendReportVO();
        vo.setDimension(dimension);
        vo.setBizType(isVehMode ? "VEHICLE" : "PEDESTRIAN");

        List<String> categories = new ArrayList<>();
        List<Integer> entryList = new ArrayList<>();
        List<Integer> exitList = new ArrayList<>();
        List<Integer> totalList = new ArrayList<>();
        int peak = 0;
        String peakTime = "-";
        int totalPeriod = 0;

        if ("realtime".equalsIgnoreCase(dimension) || "timeline".equalsIgnoreCase(dimension)) {
            LocalDate today = LocalDate.now();
            long startOfDayMs = today.atStartOfDay(ZoneId.systemDefault()).toInstant().toEpochMilli();
            long endOfDayMs = today.plusDays(1).atStartOfDay(ZoneId.systemDefault()).toInstant().toEpochMilli() - 1;

            List<BizPassageEvent> events = eventMapper.selectList(new LambdaQueryWrapper<BizPassageEvent>()
                    .eq(BizPassageEvent::getPointId, pointId)
                    .ge(BizPassageEvent::getTimestampMs, startOfDayMs)
                    .le(BizPassageEvent::getTimestampMs, endOfDayMs)
                    .orderByAsc(BizPassageEvent::getTimestampMs));

            vo.setTitle(isVehMode ? "今日实时车流通行时序曲线 (真实采集时间动态流)" : "今日实时通行时序曲线 (真实采集时间动态流)");

            if (events.isEmpty()) {
                LocalDateTime now = LocalDateTime.now();
                for (int i = 5; i >= 0; i--) {
                    categories.add(now.minusMinutes(i * 5L).format(DateTimeFormatter.ofPattern("HH:mm")));
                    entryList.add(0);
                    exitList.add(0);
                    totalList.add(0);
                }
            } else {
                long firstTs = events.get(0).getTimestampMs();
                long lastTs = events.get(events.size() - 1).getTimestampMs();
                long spanMs = Math.max(1000L, lastTs - firstTs);

                long stepMs;
                DateTimeFormatter formatter;
                if (spanMs <= 15 * 60 * 1000L) {
                    stepMs = 60 * 1000L; // 1 min
                    formatter = DateTimeFormatter.ofPattern("HH:mm");
                } else if (spanMs <= 2 * 3600 * 1000L) {
                    stepMs = 5 * 60 * 1000L; // 5 min
                    formatter = DateTimeFormatter.ofPattern("HH:mm");
                } else if (spanMs <= 6 * 3600 * 1000L) {
                    stepMs = 15 * 60 * 1000L; // 15 min
                    formatter = DateTimeFormatter.ofPattern("HH:mm");
                } else {
                    stepMs = 30 * 60 * 1000L; // 30 min
                    formatter = DateTimeFormatter.ofPattern("HH:mm");
                }

                long bucketStart = (firstTs / stepMs) * stepMs;
                long bucketEnd = (lastTs / stepMs + 1) * stepMs;

                Map<Long, int[]> bucketMap = new LinkedHashMap<>();
                for (long t = bucketStart; t <= bucketEnd; t += stepMs) {
                    bucketMap.put(t, new int[]{0, 0});
                }

                for (BizPassageEvent ev : events) {
                    long bKey = (ev.getTimestampMs() / stepMs) * stepMs;
                    int[] counts = bucketMap.computeIfAbsent(bKey, k -> new int[]{0, 0});
                    if ("ENTRY".equalsIgnoreCase(ev.getDirection())) {
                        counts[0]++;
                    } else {
                        counts[1]++;
                    }
                }

                for (Map.Entry<Long, int[]> entry : bucketMap.entrySet()) {
                    LocalDateTime ldt = LocalDateTime.ofInstant(Instant.ofEpochMilli(entry.getKey()), ZoneId.systemDefault());
                    String timeLabel = ldt.format(formatter);
                    int in = entry.getValue()[0];
                    int out = entry.getValue()[1];
                    int tot = in + out;

                    categories.add(timeLabel);
                    entryList.add(in);
                    exitList.add(out);
                    totalList.add(tot);
                    totalPeriod += tot;

                    if (tot > peak) {
                        peak = tot;
                        peakTime = timeLabel;
                    }
                }
            }
            vo.setAverageCount(categories.isEmpty() ? 0.0 : Math.round((double) totalPeriod / categories.size() * 10.0) / 10.0);

        } else if ("hour".equalsIgnoreCase(dimension)) {
            vo.setTitle(isVehMode ? "今日逐时车流分布曲线" : "今日逐时客流分布曲线");
            LocalDate today = LocalDate.now();
            List<BizStatsHourly> list = statsMapper.selectList(new LambdaQueryWrapper<BizStatsHourly>()
                    .eq(BizStatsHourly::getPointId, pointId)
                    .eq(BizStatsHourly::getStatDate, today));

            Map<Integer, BizStatsHourly> map = list.stream()
                    .collect(Collectors.toMap(BizStatsHourly::getStatHour, s -> s, (s1, s2) -> s1));

            int maxHour = Math.min(23, Math.max(LocalDateTime.now().getHour(), 6));
            for (int h = 0; h <= maxHour; h++) {
                categories.add(String.format("%02d:00", h));
                BizStatsHourly s = map.get(h);
                int in = s != null ? s.getEntryCount() : 0;
                int out = s != null ? s.getExitCount() : 0;
                int tot = in + out;
                entryList.add(in);
                exitList.add(out);
                totalList.add(tot);
                totalPeriod += tot;
                if (tot > peak) {
                    peak = tot;
                    peakTime = String.format("%02d:00", h);
                }
            }
            vo.setAverageCount(Math.round((double) totalPeriod / Math.max(1, maxHour + 1) * 10.0) / 10.0);

        } else if ("day".equalsIgnoreCase(dimension)) {
            int d = (days != null && days > 0) ? days : 7;
            vo.setTitle(String.format(isVehMode ? "近 %d 天逐日车流走势曲线" : "近 %d 天逐日客流走势曲线", d));
            LocalDate endDate = LocalDate.now();
            LocalDate startDate = endDate.minusDays(d - 1);

            List<Map<String, Object>> rows = statsMapper.selectDailyStats(pointId, startDate, endDate);
            Map<String, Map<String, Object>> dateMap = new HashMap<>();
            if (rows != null) {
                for (Map<String, Object> r : rows) {
                    Object dt = r.get("statDate");
                    if (dt != null) dateMap.put(dt.toString(), r);
                }
            }

            for (int i = 0; i < d; i++) {
                LocalDate cur = startDate.plusDays(i);
                String dtStr = cur.toString();
                String label = cur.format(DateTimeFormatter.ofPattern("MM-dd"));
                categories.add(label);

                Map<String, Object> r = dateMap.get(dtStr);
                int in = r != null && r.get("entryCount") != null ? ((Number) r.get("entryCount")).intValue() : 0;
                int out = r != null && r.get("exitCount") != null ? ((Number) r.get("exitCount")).intValue() : 0;
                int tot = in + out;

                entryList.add(in);
                exitList.add(out);
                totalList.add(tot);
                totalPeriod += tot;
                if (tot > peak) {
                    peak = tot;
                    peakTime = label;
                }
            }
            vo.setAverageCount(Math.round((double) totalPeriod / d * 10.0) / 10.0);

        } else if ("quarter".equalsIgnoreCase(dimension)) {
            vo.setTitle(isVehMode ? "年度季度车流走势曲线" : "年度季度客流走势曲线");
            List<Map<String, Object>> rows = statsMapper.selectQuarterlyStats(pointId);
            if (rows != null && !rows.isEmpty()) {
                for (Map<String, Object> r : rows) {
                    String qName = r.get("quarterName") != null ? r.get("quarterName").toString() : "-";
                    int in = r.get("entryCount") != null ? ((Number) r.get("entryCount")).intValue() : 0;
                    int out = r.get("exitCount") != null ? ((Number) r.get("exitCount")).intValue() : 0;
                    int tot = in + out;

                    categories.add(qName);
                    entryList.add(in);
                    exitList.add(out);
                    totalList.add(tot);
                    totalPeriod += tot;
                    if (tot > peak) {
                        peak = tot;
                        peakTime = qName;
                    }
                }
            } else {
                categories.addAll(Arrays.asList("Q1", "Q2", "Q3", "Q4"));
                entryList.addAll(Arrays.asList(0, 0, 0, 0));
                exitList.addAll(Arrays.asList(0, 0, 0, 0));
                totalList.addAll(Arrays.asList(0, 0, 0, 0));
            }
            vo.setAverageCount(categories.isEmpty() ? 0.0 : Math.round((double) totalPeriod / categories.size() * 10.0) / 10.0);
        }

        vo.setCategories(categories);
        vo.setEntryList(entryList);
        vo.setExitList(exitList);
        vo.setTotalList(totalList);
        vo.setPeakValue(peak);
        vo.setPeakTime(peakTime);
        vo.setTotalPeriodCount(totalPeriod);

        return vo;
    }

    @Override
    public Page<EventItemVO> pageEvents(Long userId, Long pointId, String startDate, String endDate, String direction, int page, int size) {
        Page<BizPassageEvent> eventPage = new Page<>(page, size);
        LambdaQueryWrapper<BizPassageEvent> wrapper = new LambdaQueryWrapper<>();

        if (pointId != null && pointId > 0) {
            wrapper.eq(BizPassageEvent::getPointId, pointId);
        } else if (userId != null) {
            SysUser user = userMapper.selectById(userId);
            if (user != null && !"ROLE_ADMIN".equals(user.getRole())) {
                List<BizPoint> points = pointMapper.selectList(new LambdaQueryWrapper<BizPoint>().eq(BizPoint::getUserId, userId));
                if (points.isEmpty()) {
                    return new Page<>(page, size, 0);
                }
                List<Long> pids = points.stream().map(BizPoint::getId).collect(Collectors.toList());
                wrapper.in(BizPassageEvent::getPointId, pids);
            }
        }

        if (startDate != null && !startDate.trim().isEmpty()) {
            try {
                LocalDate sDate = LocalDate.parse(startDate.trim());
                long startMs = sDate.atStartOfDay(ZoneId.systemDefault()).toInstant().toEpochMilli();
                wrapper.ge(BizPassageEvent::getTimestampMs, startMs);
            } catch (Exception ignored) {}
        }
        if (endDate != null && !endDate.trim().isEmpty()) {
            try {
                LocalDate eDate = LocalDate.parse(endDate.trim());
                long endMs = eDate.plusDays(1).atStartOfDay(ZoneId.systemDefault()).toInstant().toEpochMilli() - 1;
                wrapper.le(BizPassageEvent::getTimestampMs, endMs);
            } catch (Exception ignored) {}
        }

        if (direction != null && !direction.trim().isEmpty() && !"ALL".equalsIgnoreCase(direction)) {
            wrapper.eq(BizPassageEvent::getDirection, direction.trim().toUpperCase());
        }

        wrapper.orderByDesc(BizPassageEvent::getTimestampMs);

        Page<BizPassageEvent> res = eventMapper.selectPage(eventPage, wrapper);
        Page<EventItemVO> voPage = new Page<>(res.getCurrent(), res.getSize(), res.getTotal());

        Map<Long, String> pointNameMap = new HashMap<>();

        voPage.setRecords(res.getRecords().stream().map(e -> {
            EventItemVO vo = new EventItemVO();
            vo.setId(e.getId());
            vo.setPointId(e.getPointId());

            String pName = pointNameMap.computeIfAbsent(e.getPointId(), pid -> {
                BizPoint p = pointMapper.selectById(pid);
                return p != null ? p.getPointName() : ("通道-" + pid);
            });
            vo.setPointName(pName);

            vo.setTrackId(e.getTrackId());
            vo.setDirection(e.getDirection());
            vo.setConfidence(e.getConfidence());
            vo.setGender(e.getGender());
            vo.setAgeGroup(e.getAgeGroup());
            vo.setPlateNumber(e.getPlateNumber());
            vo.setPlateColor(e.getPlateColor());
            vo.setVehicleType(e.getVehicleType());
            vo.setVehicleColor(e.getVehicleColor());
            vo.setTimestampMs(e.getTimestampMs());

            LocalDateTime ldt = LocalDateTime.ofInstant(Instant.ofEpochMilli(e.getTimestampMs()), ZoneId.systemDefault());
            vo.setDatetime(ldt.format(DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss")));

            vo.setPlayUrl(minioService.getPresignedPlayUrl(e.getClipUrl()));
            return vo;
        }).collect(Collectors.toList()));

        return voPage;
    }
}
