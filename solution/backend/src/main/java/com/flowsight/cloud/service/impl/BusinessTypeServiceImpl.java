package com.flowsight.cloud.service.impl;

import com.baomidou.mybatisplus.core.conditions.query.LambdaQueryWrapper;
import com.baomidou.mybatisplus.extension.plugins.pagination.Page;
import com.fasterxml.jackson.core.type.TypeReference;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.flowsight.cloud.dto.BusinessTypeDTO;
import com.flowsight.cloud.entity.BusinessType;
import com.flowsight.cloud.mapper.BusinessTypeMapper;
import com.flowsight.cloud.service.BusinessTypeService;
import com.flowsight.cloud.vo.BusinessTypeOptionVO;
import com.flowsight.cloud.vo.BusinessTypeVO;
import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.BeanUtils;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.util.ArrayList;
import java.util.List;
import java.util.stream.Collectors;

@Slf4j
@Service
public class BusinessTypeServiceImpl implements BusinessTypeService {

    @Autowired
    private BusinessTypeMapper businessTypeMapper;

    @Autowired
    private ObjectMapper objectMapper;

    @Override
    public Page<BusinessTypeVO> pageBusinessTypes(int page, int size, String keyword) {
        Page<BusinessType> mpPage = new Page<>(page, size);
        LambdaQueryWrapper<BusinessType> wrapper = new LambdaQueryWrapper<BusinessType>()
                .like(keyword != null && !keyword.trim().isEmpty(), BusinessType::getBizName, keyword)
                .or(keyword != null && !keyword.trim().isEmpty(), w -> w.like(BusinessType::getBizCode, keyword).or().like(BusinessType::getDescription, keyword))
                .orderByAsc(BusinessType::getSortOrder)
                .orderByAsc(BusinessType::getId);

        Page<BusinessType> result = businessTypeMapper.selectPage(mpPage, wrapper);
        Page<BusinessTypeVO> voPage = new Page<>(result.getCurrent(), result.getSize(), result.getTotal());
        List<BusinessTypeVO> voList = result.getRecords().stream()
                .map(this::convertToVO)
                .collect(Collectors.toList());
        voPage.setRecords(voList);
        return voPage;
    }

    @Override
    public List<BusinessTypeOptionVO> listActiveOptions() {
        LambdaQueryWrapper<BusinessType> wrapper = new LambdaQueryWrapper<BusinessType>()
                .eq(BusinessType::getStatus, 1)
                .orderByAsc(BusinessType::getSortOrder)
                .orderByAsc(BusinessType::getId);

        List<BusinessType> list = businessTypeMapper.selectList(wrapper);
        return list.stream().map(type -> {
            List<String> featureList = parseFeatures(type.getFeatures());
            return BusinessTypeOptionVO.builder()
                    .bizCode(type.getBizCode())
                    .bizName(type.getBizName())
                    .description(type.getDescription())
                    .featureList(featureList)
                    .deviceReq(type.getDeviceReq())
                    .icon(type.getIcon())
                    .colorTag(type.getColorTag())
                    .build();
        }).collect(Collectors.toList());
    }

    @Override
    @Transactional(rollbackFor = Exception.class)
    public BusinessTypeVO createBusinessType(BusinessTypeDTO dto) {
        String code = dto.getBizCode().trim().toUpperCase();
        // 检查唯一性
        Long count = businessTypeMapper.selectCount(new LambdaQueryWrapper<BusinessType>()
                .eq(BusinessType::getBizCode, code));
        if (count != null && count > 0) {
            throw new IllegalArgumentException("业务编码 [" + code + "] 已存在，请更换其他唯一英文代号");
        }

        BusinessType type = new BusinessType();
        BeanUtils.copyProperties(dto, type);
        type.setBizCode(code);
        type.setFeatures(serializeFeatures(dto.getFeatureList()));
        if (type.getStatus() == null) type.setStatus(1);
        if (type.getSortOrder() == null) type.setSortOrder(0);
        if (type.getIcon() == null || type.getIcon().trim().isEmpty()) type.setIcon("Operation");
        if (type.getColorTag() == null || type.getColorTag().trim().isEmpty()) type.setColorTag("#388bfd");
        type.setIsSystem(0);

        businessTypeMapper.insert(type);
        log.info("[BusinessType] 成功创建业务类型: id={}, code={}, name={}", type.getId(), type.getBizCode(), type.getBizName());
        return convertToVO(type);
    }

    @Override
    @Transactional(rollbackFor = Exception.class)
    public void updateBusinessType(Long id, BusinessTypeDTO dto) {
        BusinessType type = businessTypeMapper.selectById(id);
        if (type == null) {
            throw new IllegalArgumentException("目标业务类型不存在 (ID: " + id + ")");
        }

        String newCode = dto.getBizCode().trim().toUpperCase();
        // 系统内置业务禁止修改业务编码
        if (Integer.valueOf(1).equals(type.getIsSystem()) && !type.getBizCode().equalsIgnoreCase(newCode)) {
            throw new IllegalArgumentException("系统内置原生业务类型禁止篡改核心业务编码 [" + type.getBizCode() + "]");
        }

        // 若修改了业务编码，校验新编码是否与其他记录冲突
        if (!type.getBizCode().equalsIgnoreCase(newCode)) {
            Long count = businessTypeMapper.selectCount(new LambdaQueryWrapper<BusinessType>()
                    .eq(BusinessType::getBizCode, newCode)
                    .ne(BusinessType::getId, id));
            if (count != null && count > 0) {
                throw new IllegalArgumentException("业务编码 [" + newCode + "] 已被其他业务使用");
            }
            type.setBizCode(newCode);
        }

        type.setBizName(dto.getBizName());
        type.setDescription(dto.getDescription());
        type.setFeatures(serializeFeatures(dto.getFeatureList()));
        type.setDeviceReq(dto.getDeviceReq());
        if (dto.getIcon() != null) type.setIcon(dto.getIcon());
        if (dto.getColorTag() != null) type.setColorTag(dto.getColorTag());
        if (dto.getStatus() != null) type.setStatus(dto.getStatus());
        if (dto.getSortOrder() != null) type.setSortOrder(dto.getSortOrder());

        businessTypeMapper.updateById(type);
        log.info("[BusinessType] 成功更新业务类型: id={}, code={}, name={}", type.getId(), type.getBizCode(), type.getBizName());
    }

    @Override
    @Transactional(rollbackFor = Exception.class)
    public void deleteBusinessType(Long id) {
        BusinessType type = businessTypeMapper.selectById(id);
        if (type == null) {
            throw new IllegalArgumentException("目标业务类型不存在或已被删除");
        }

        if (Integer.valueOf(1).equals(type.getIsSystem())) {
            throw new IllegalArgumentException("系统内置业务类型 [" + type.getBizName() + "] 属于核心骨干基础模式，禁止直接删除！");
        }

        // 安全校验：检查是否有在使用的点位关联了该业务模式
        int activePoints = businessTypeMapper.countActivePointsByBizType(type.getBizCode());
        if (activePoints > 0) {
            throw new IllegalArgumentException(String.format("无法删除业务 [%s]：当前系统中尚有 %d 个监控点位正在运行绑定该业务，请先解绑或迁移点位业务！",
                    type.getBizName(), activePoints));
        }

        businessTypeMapper.deleteById(id);
        log.info("[BusinessType] 成功删除业务类型: id={}, code={}, name={}", type.getId(), type.getBizCode(), type.getBizName());
    }

    @Override
    @Transactional(rollbackFor = Exception.class)
    public void toggleStatus(Long id, Integer status) {
        BusinessType type = businessTypeMapper.selectById(id);
        if (type == null) {
            throw new IllegalArgumentException("业务类型不存在");
        }
        type.setStatus(status != null && status == 1 ? 1 : 0);
        businessTypeMapper.updateById(type);
        log.info("[BusinessType] 业务类型状态切换: id={}, code={}, status={}", type.getId(), type.getBizCode(), type.getStatus());
    }

    @Override
    public BusinessTypeVO getById(Long id) {
        BusinessType type = businessTypeMapper.selectById(id);
        if (type == null) {
            throw new IllegalArgumentException("业务类型不存在");
        }
        return convertToVO(type);
    }

    private BusinessTypeVO convertToVO(BusinessType entity) {
        BusinessTypeVO vo = new BusinessTypeVO();
        BeanUtils.copyProperties(entity, vo);
        vo.setFeatureList(parseFeatures(entity.getFeatures()));
        vo.setPointCount(businessTypeMapper.countActivePointsByBizType(entity.getBizCode()));
        return vo;
    }

    private List<String> parseFeatures(String json) {
        if (json == null || json.trim().isEmpty()) {
            return new ArrayList<>();
        }
        try {
            return objectMapper.readValue(json, new TypeReference<List<String>>() {});
        } catch (Exception e) {
            log.warn("[BusinessType] 特征属性 JSON 解析异常, 降级为空列表: json={}, err={}", json, e.getMessage());
            return new ArrayList<>();
        }
    }

    private String serializeFeatures(List<String> list) {
        if (list == null || list.isEmpty()) {
            return "[]";
        }
        try {
            return objectMapper.writeValueAsString(list);
        } catch (Exception e) {
            log.error("[BusinessType] 特征属性列表序列化异常: {}", e.getMessage());
            return "[]";
        }
    }
}
