package com.flowsight.cloud.mapper;

import com.baomidou.mybatisplus.core.mapper.BaseMapper;
import com.flowsight.cloud.entity.BizPassageEvent;
import org.apache.ibatis.annotations.Insert;
import org.apache.ibatis.annotations.Mapper;
import org.apache.ibatis.annotations.Param;

import java.util.List;

@Mapper
public interface BizPassageEventMapper extends BaseMapper<BizPassageEvent> {

    /**
     * 单语句多值高性能批量写入 (将多条插入合并为 1 次数据库事务与网络往返)
     */
    @Insert("<script>" +
            "INSERT INTO `biz_passage_event` " +
            "(`point_id`, `user_id`, `track_id`, `direction`, `confidence`, `gender`, `age_group`, " +
            "`plate_number`, `plate_color`, `vehicle_type`, `vehicle_color`, `timestamp_ms`, `local_clip_path`, `clip_url`) " +
            "VALUES " +
            "<foreach collection='list' item='item' separator=','>" +
            "(#{item.pointId}, #{item.userId}, #{item.trackId}, #{item.direction}, #{item.confidence}, " +
            "#{item.gender}, #{item.ageGroup}, #{item.plateNumber}, #{item.plateColor}, #{item.vehicleType}, " +
            "#{item.vehicleColor}, #{item.timestampMs}, #{item.localClipPath}, #{item.clipUrl})" +
            "</foreach>" +
            "</script>")
    int insertBatch(@Param("list") List<BizPassageEvent> list);
}
