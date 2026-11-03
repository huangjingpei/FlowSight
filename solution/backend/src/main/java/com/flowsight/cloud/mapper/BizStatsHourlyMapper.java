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

    @Select("SELECT " +
            "COALESCE(SUM(CASE WHEN stat_date >= #{monthStart} AND stat_date <= #{today} THEN entry_count ELSE 0 END), 0) as monthEntry, " +
            "COALESCE(SUM(CASE WHEN stat_date >= #{yearStart} AND stat_date <= #{today} THEN entry_count ELSE 0 END), 0) as yearEntry, " +
            "COALESCE(SUM(entry_count), 0) as historicalEntry " +
            "FROM biz_stats_hourly " +
            "WHERE point_id = #{pointId}")
    Map<String, Object> selectPeriodicStatsByPoint(@Param("pointId") Long pointId,
                                                  @Param("monthStart") LocalDate monthStart,
                                                  @Param("yearStart") LocalDate yearStart,
                                                  @Param("today") LocalDate today);

    @Select("<script>" +
            "SELECT point_id as pointId, " +
            "COALESCE(SUM(CASE WHEN stat_date &gt;= #{monthStart} AND stat_date &lt;= #{today} THEN entry_count ELSE 0 END), 0) as monthEntry, " +
            "COALESCE(SUM(CASE WHEN stat_date &gt;= #{yearStart} AND stat_date &lt;= #{today} THEN entry_count ELSE 0 END), 0) as yearEntry, " +
            "COALESCE(SUM(entry_count), 0) as historicalEntry " +
            "FROM biz_stats_hourly " +
            "WHERE point_id IN " +
            "<foreach item='item' collection='pointIds' open='(' separator=',' close=')'>" +
            "#{item}" +
            "</foreach> " +
            "GROUP BY point_id" +
            "</script>")
    List<Map<String, Object>> selectPeriodicStatsBatch(@Param("pointIds") List<Long> pointIds,
                                                       @Param("monthStart") LocalDate monthStart,
                                                       @Param("yearStart") LocalDate yearStart,
                                                       @Param("today") LocalDate today);
}
