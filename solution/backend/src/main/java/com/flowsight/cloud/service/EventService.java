package com.flowsight.cloud.service;

import com.baomidou.mybatisplus.extension.plugins.pagination.Page;
import com.flowsight.cloud.dto.EdgeEventDTO;
import com.flowsight.cloud.dto.EdgeHeartbeatDTO;
import com.flowsight.cloud.vo.DashboardSummaryVO;
import com.flowsight.cloud.vo.EventItemVO;
import org.springframework.web.multipart.MultipartFile;

public interface EventService {
    void recordEdgeEvent(Long pointId, EdgeEventDTO dto);
    String uploadEdgeClip(Long pointId, Integer trackId, Long timestamp, MultipartFile file);
    void recordHeartbeat(Long pointId, EdgeHeartbeatDTO dto);
    DashboardSummaryVO getDashboardSummary(Long pointId);
    Page<EventItemVO> pageEvents(Long pointId, int page, int size);
}
