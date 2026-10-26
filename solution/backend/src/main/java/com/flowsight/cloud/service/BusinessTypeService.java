package com.flowsight.cloud.service;

import com.baomidou.mybatisplus.extension.plugins.pagination.Page;
import com.flowsight.cloud.dto.BusinessTypeDTO;
import com.flowsight.cloud.vo.BusinessTypeOptionVO;
import com.flowsight.cloud.vo.BusinessTypeVO;

import java.util.List;

public interface BusinessTypeService {
    Page<BusinessTypeVO> pageBusinessTypes(int page, int size, String keyword);
    List<BusinessTypeOptionVO> listActiveOptions();
    BusinessTypeVO createBusinessType(BusinessTypeDTO dto);
    void updateBusinessType(Long id, BusinessTypeDTO dto);
    void deleteBusinessType(Long id);
    void toggleStatus(Long id, Integer status);
    BusinessTypeVO getById(Long id);
}
