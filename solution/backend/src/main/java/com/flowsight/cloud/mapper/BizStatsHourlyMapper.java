package com.flowsight.cloud.mapper;

import com.baomidou.mybatisplus.core.mapper.BaseMapper;
import com.flowsight.cloud.entity.BizStatsHourly;
import org.apache.ibatis.annotations.Mapper;
import org.apache.ibatis.annotations.Param;
import org.apache.ibatis.annotations.Select;

import java.time.LocalDate;
import java.util.List;
import java.util.Map;

@Mapper
public interface BizStatsHourlyMapper extends BaseMapper<BizStatsHourly> {

    @Select("SELECT stat_date as statDate, SUM(entry_count) as entryCount, SUM(exit_count) as exitCount " +
            "FROM biz_stats_hourly WHERE point_id = #{pointId} AND stat_date >= #{startDate} AND stat_date <= #{endDate} " +
            "GROUP BY stat_date ORDER BY stat_date ASC")
    List<Map<String, Object>> selectDailyStats(@Param("pointId") Long pointId,
                                               @Param("startDate") LocalDate startDate,
                                               @Param("endDate") LocalDate endDate);

    @Select("SELECT CONCAT(YEAR(stat_date), '-Q', QUARTER(stat_date)) as quarterName, " +
            "SUM(entry_count) as entryCount, SUM(exit_count) as exitCount " +
            "FROM biz_stats_hourly WHERE point_id = #{pointId} " +
            "GROUP BY YEAR(stat_date), QUARTER(stat_date) ORDER BY YEAR(stat_date) ASC, QUARTER(stat_date) ASC")
    List<Map<String, Object>> selectQuarterlyStats(@Param("pointId") Long pointId);
}
