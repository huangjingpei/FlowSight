package com.flowsight.cloud.service;

import com.baomidou.mybatisplus.extension.plugins.pagination.Page;
import com.flowsight.cloud.dto.EdgeBatchReportDTO;
import com.flowsight.cloud.dto.EdgeEventDTO;
import com.flowsight.cloud.dto.EdgeHeartbeatDTO;
import com.flowsight.cloud.vo.DashboardSummaryVO;
import com.flowsight.cloud.vo.EventItemVO;
import com.flowsight.cloud.vo.TrendReportVO;
import org.springframework.web.multipart.MultipartFile;

public interface EventService {
    void recordEdgeEvent(Long pointId, EdgeEventDTO dto);
    void recordEdgeBatchReport(Long pointId, EdgeBatchReportDTO dto);
    String uploadEdgeClip(Long pointId, Integer trackId, Long timestamp, MultipartFile file);
    void recordHeartbeat(Long pointId, EdgeHeartbeatDTO dto);
    DashboardSummaryVO getDashboardSummary(Long pointId);
    TrendReportVO getTrendReport(Long pointId, String dimension, Integer days);
    Page<EventItemVO> pageEvents(Long userId, Long pointId, String startDate, String endDate, String direction, int page, int size);
}
