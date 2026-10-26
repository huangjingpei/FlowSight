package com.flowsight.cloud.config;

import jakarta.annotation.PostConstruct;
import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.context.annotation.Configuration;

import javax.sql.DataSource;
import java.sql.Connection;
import java.sql.ResultSet;
import java.sql.Statement;

/**
 * 数据库表结构自动化向后兼容演进升级配置
 * 解决新功能上线 (如人车双流二选一与车牌识别) 时，历史运行中 MySQL 数据库缺少新字段导致的 BadSqlGrammarException 崩溃
 */
@Slf4j
@Configuration
public class DatabaseSchemaMigrationConfig {

    @Autowired
    private DataSource dataSource;

    @PostConstruct
    public void autoMigrateSchema() {
        log.info("[DatabaseMigration] 正在核验并自动升级数据库表结构 (自动向下兼容扩展)...");
        try (Connection conn = dataSource.getConnection();
             Statement stmt = conn.createStatement()) {

            // 1. 检查 biz_point 表中是否存在 biz_type 字段
            boolean hasBizType = false;
            try (ResultSet rs = stmt.executeQuery(
                    "SELECT COUNT(*) FROM information_schema.COLUMNS " +
                    "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'biz_point' AND COLUMN_NAME = 'biz_type'")) {
                if (rs.next() && rs.getInt(1) > 0) {
                    hasBizType = true;
                }
            }

            if (!hasBizType) {
                log.info("[DatabaseMigration] biz_point 缺失 biz_type 字段，正在执行热补丁添加...");
                stmt.executeUpdate("ALTER TABLE biz_point ADD COLUMN `biz_type` VARCHAR(16) NOT NULL DEFAULT 'PEDESTRIAN' " +
                        "COMMENT '业务模式: PEDESTRIAN-客流, VEHICLE-车流' AFTER `resolution`");
                log.info("[DatabaseMigration] biz_point.biz_type 字段添加成功！");

                // 将点位 2 (万科西区地库机动车道) 默认设为 VEHICLE 车流感知点位
                stmt.executeUpdate("UPDATE biz_point SET biz_type = 'VEHICLE' WHERE id = 2 OR point_code = 'POINT_VK_BASE_02'");
                log.info("[DatabaseMigration] 点位 2 业务模式已自动置为 VEHICLE 车流模式！");
            }

            // 2. 检查 biz_passage_event 表中是否存在 plate_number 字段
            boolean hasPlateNumber = false;
            try (ResultSet rs = stmt.executeQuery(
                    "SELECT COUNT(*) FROM information_schema.COLUMNS " +
                    "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'biz_passage_event' AND COLUMN_NAME = 'plate_number'")) {
                if (rs.next() && rs.getInt(1) > 0) {
                    hasPlateNumber = true;
                }
            }

            if (!hasPlateNumber) {
                log.info("[DatabaseMigration] biz_passage_event 缺失车牌与车型字段，正在执行热补丁添加...");
                stmt.executeUpdate("ALTER TABLE biz_passage_event " +
                        "ADD COLUMN `plate_number` VARCHAR(32) DEFAULT '' COMMENT '车牌号码' AFTER `age_group`, " +
                        "ADD COLUMN `plate_color` VARCHAR(16) DEFAULT '' COMMENT '号牌颜色' AFTER `plate_number`, " +
                        "ADD COLUMN `vehicle_type` VARCHAR(32) DEFAULT '' COMMENT '车辆类型' AFTER `plate_color`, " +
                        "ADD COLUMN `vehicle_color` VARCHAR(16) DEFAULT '' COMMENT '车身颜色' AFTER `vehicle_type`");
                log.info("[DatabaseMigration] biz_passage_event 车牌与车型字段添加成功！");
            }

            // 3. 检查是否有针对 plate_number 的索引
            boolean hasPlateIdx = false;
            try (ResultSet rs = stmt.executeQuery(
                    "SELECT COUNT(*) FROM information_schema.STATISTICS " +
                    "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'biz_passage_event' AND INDEX_NAME = 'idx_plate'")) {
                if (rs.next() && rs.getInt(1) > 0) {
                    hasPlateIdx = true;
                }
            }
            if (!hasPlateIdx) {
                stmt.executeUpdate("ALTER TABLE biz_passage_event ADD INDEX `idx_plate` (`plate_number`)");
                log.info("[DatabaseMigration] biz_passage_event.idx_plate 索引创建成功！");
            }

            log.info("[DatabaseMigration] 数据库表结构自愈与演进升级完成！");
        } catch (Exception e) {
            log.error("[DatabaseMigration] 数据库自动升级失败: ", e);
        }
    }
}
