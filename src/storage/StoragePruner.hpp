#pragma once

#include <QString>

/**
 * @brief 磁盘存储保护与过期短视频切片自动清理服务
 */
class StoragePruner {
public:
    /**
     * @brief 扫描切片目录并静默清理早于保留天数的视频文件
     * @param clips_dir 视频切片根目录 (默认 "clips")
     * @param retention_days 允许保留的最大天数 (超出将被物理删除以释放空间)
     * @return 实际清理的文件数量
     */
    static int prune_expired_clips(const QString& clips_dir = "clips", int retention_days = 30);
};
