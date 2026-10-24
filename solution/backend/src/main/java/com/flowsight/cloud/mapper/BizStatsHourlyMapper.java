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

    @Select("SELECT t.quarterName, SUM(t.entry_count) as entryCount, SUM(t.exit_count) as exitCount " +
            "FROM (SELECT CONCAT(YEAR(stat_date), '-Q', QUARTER(stat_date)) as quarterName, entry_count, exit_count " +
            "      FROM biz_stats_hourly WHERE point_id = #{pointId}) t " +
            "GROUP BY t.quarterName ORDER BY t.quarterName ASC")
    List<Map<String, Object>> selectQuarterlyStats(@Param("pointId") Long pointId);
}
