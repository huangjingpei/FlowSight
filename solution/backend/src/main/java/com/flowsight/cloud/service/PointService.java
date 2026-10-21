package com.flowsight.cloud.service;

import com.baomidou.mybatisplus.extension.plugins.pagination.Page;
import com.flowsight.cloud.dto.PointCreateDTO;
import com.flowsight.cloud.dto.PointUpdateDTO;
import com.flowsight.cloud.entity.BizPoint;
import com.flowsight.cloud.vo.PointVO;

import java.util.List;

public interface PointService {
    Page<PointVO> pagePoints(int page, int size, Long userId, String keyword);
    List<PointVO> listUserPoints(Long userId);
    PointVO createPoint(PointCreateDTO dto);
    void updatePoint(Long id, PointUpdateDTO dto);
    void deletePointCascadeCloud(Long id);
    PointVO getPointById(Long id);
    BizPoint getPointByToken(String token);
    String refreshToken(Long id);
}
