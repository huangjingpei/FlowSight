#include "StoragePruner.hpp"
#include "DatabaseManager.hpp"
#include <QDir>
#include <QFileInfoList>
#include <QDateTime>
#include <QDebug>

int StoragePruner::prune_expired_clips(const QString& clips_dir, int retention_days) {
    if (retention_days <= 0) return 0;

    QDir dir(clips_dir);
    if (!dir.exists()) {
        return 0;
    }

    QDateTime threshold = QDateTime::currentDateTime().addDays(-retention_days);
    QFileInfoList file_list = dir.entryInfoList(QStringList() << "*.mp4" << "*.mkv" << "*.avi", QDir::Files);

    int deleted_count = 0;
    qint64 freed_bytes = 0;

    for (const auto& fi : file_list) {
        if (fi.lastModified() < threshold) {
            qint64 size = fi.size();
            if (QFile::remove(fi.absoluteFilePath())) {
                deleted_count++;
                freed_bytes += size;
            }
        }
    }

    if (deleted_count > 0) {
        double freed_mb = freed_bytes / (1024.0 * 1024.0);
        qInfo() << QString("[StoragePruner] 成功自动清理 %1 个早于 %2 天的过期切片，释放磁盘空间: %3 MB")
                   .arg(deleted_count)
                   .arg(retention_days)
                   .arg(QString::number(freed_mb, 'f', 2));

        // 同步清理 SQLite 数据库中已失效的切片文件路径，杜绝孤儿死链
        DatabaseManager::instance().prune_expired_event_clips(threshold.toMSecsSinceEpoch());
    }

    return deleted_count;
}

int StoragePruner::clear_all_clips(const QString& clips_dir, qint64* out_freed_bytes) {
    QDir dir(clips_dir);
    if (!dir.exists()) {
        if (out_freed_bytes) *out_freed_bytes = 0;
        return 0;
    }

    QFileInfoList file_list = dir.entryInfoList(QStringList() << "*.mp4" << "*.mkv" << "*.avi" << "*.tmp", QDir::Files);
    int deleted_count = 0;
    qint64 freed_bytes = 0;

    for (const auto& fi : file_list) {
        qint64 size = fi.size();
        if (QFile::remove(fi.absoluteFilePath())) {
            deleted_count++;
            freed_bytes += size;
        }
    }

    if (out_freed_bytes) {
        *out_freed_bytes = freed_bytes;
    }

    double freed_mb = freed_bytes / (1024.0 * 1024.0);
    qInfo() << QString("[StoragePruner] 一键全量清空切片完成，共删除 %1 个视频文件，释放空间: %2 MB")
               .arg(deleted_count)
               .arg(QString::number(freed_mb, 'f', 2));

    return deleted_count;
}

