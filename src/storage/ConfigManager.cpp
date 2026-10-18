#include "ConfigManager.hpp"
#include <QFileInfo>
#include <QDir>
#include <QDebug>

ConfigManager& ConfigManager::instance() {
    static ConfigManager inst;
    return inst;
}

ConfigManager::ConfigManager() {
    load();
}

void ConfigManager::load(const QString& ini_path) {
    ini_path_ = ini_path;
    QFileInfo fi(ini_path_);
    if (!fi.exists()) {
        // 若配置文件不存在，默认保存初始配置以生成标准模板
        save();
        return;
    }

    QSettings settings(ini_path_, QSettings::IniFormat);
    video_source_ = settings.value("Video/Source", "configs/test_corridor.mp4").toString();
    clip_retention_days_ = settings.value("Storage/RetentionDays", 30).toInt();
    conf_thresh_ = settings.value("AI/ConfThreshold", 0.45f).toFloat();
    nms_thresh_ = settings.value("AI/NmsThreshold", 0.50f).toFloat();
    buffer_dist_ = settings.value("ROI/BufferDistance", 20.0f).toFloat();
    confirm_frames_ = settings.value("ROI/ConfirmFrames", 4).toInt();
    audio_volume_ = settings.value("Media/Volume", 80).toInt();
}

void ConfigManager::save() {
    QFileInfo fi(ini_path_);
    QDir dir = fi.dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QSettings settings(ini_path_, QSettings::IniFormat);
    settings.setValue("Video/Source", video_source_);
    settings.setValue("Storage/RetentionDays", clip_retention_days_);
    settings.setValue("AI/ConfThreshold", conf_thresh_);
    settings.setValue("AI/NmsThreshold", nms_thresh_);
    settings.setValue("ROI/BufferDistance", buffer_dist_);
    settings.setValue("ROI/ConfirmFrames", confirm_frames_);
    settings.setValue("Media/Volume", audio_volume_);
    settings.sync();

    emit config_changed();
}
