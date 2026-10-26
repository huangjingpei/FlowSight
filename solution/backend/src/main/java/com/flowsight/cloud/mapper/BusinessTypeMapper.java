package com.flowsight.cloud.mapper;

import com.baomidou.mybatisplus.core.mapper.BaseMapper;
import com.flowsight.cloud.entity.BusinessType;
import org.apache.ibatis.annotations.Mapper;
import org.apache.ibatis.annotations.Param;
import org.apache.ibatis.annotations.Select;

@Mapper
public interface BusinessTypeMapper extends BaseMapper<BusinessType> {

    /**
     * 统计当前业务类型下正在使用的有效点位数 (is_deleted = 0)
     */
    @Select("SELECT COUNT(*) FROM biz_point WHERE biz_type = #{bizCode} AND is_deleted = 0")
    int countActivePointsByBizType(@Param("bizCode") String bizCode);
}
