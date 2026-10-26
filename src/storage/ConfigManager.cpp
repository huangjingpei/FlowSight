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
    video_source_ = settings.value("Video/Source", "").toString();
    clip_retention_days_ = settings.value("Storage/RetentionDays", 30).toInt();
    conf_thresh_ = settings.value("AI/ConfThreshold", 0.45f).toFloat();
    nms_thresh_ = settings.value("AI/NmsThreshold", 0.50f).toFloat();
    buffer_dist_ = settings.value("ROI/BufferDistance", 20.0f).toFloat();
    confirm_frames_ = settings.value("ROI/ConfirmFrames", 4).toInt();
    audio_volume_ = settings.value("Media/Volume", 80).toInt();

    crowd_surge_enabled_ = settings.value("Alert/CrowdSurgeEnabled", true).toBool();
    crowd_surge_threshold_ = settings.value("Alert/CrowdSurgeThreshold", 10).toInt();
    crowd_surge_beep_ = settings.value("Alert/CrowdSurgeBeep", false).toBool();

    webhook_enabled_ = settings.value("Webhook/Enabled", false).toBool();
    webhook_url_ = settings.value("Webhook/Url", "").toString();
    webhook_token_ = settings.value("Webhook/Token", "").toString();
    webhook_timeout_ms_ = settings.value("Webhook/TimeoutMs", 3000).toInt();

    cloud_server_url_ = settings.value("Cloud/ServerUrl", "http://localhost:8080").toString();
    QString default_host = QString("FlowSight-%1").arg(QSysInfo::machineHostName());
    cloud_client_name_ = settings.value("Cloud/ClientName", default_host).toString();
    if (cloud_client_name_.isEmpty() || cloud_client_name_ == "FlowSight-Node-1") {
        cloud_client_name_ = default_host;
    }
    cloud_user_phone_ = settings.value("Cloud/UserPhone", "").toString();
    cloud_company_name_ = settings.value("Cloud/CompanyName", "").toString();
    cloud_point_id_ = settings.value("Cloud/PointId", 0).toLongLong();
    cloud_point_name_ = settings.value("Cloud/PointName", "").toString();
    cloud_point_code_ = settings.value("Cloud/PointCode", "").toString();
    cloud_device_password_ = settings.value("Cloud/DevicePassword", "").toString();
    cloud_point_token_ = settings.value("Cloud/PointToken", "").toString();
    cloud_sync_enabled_ = settings.value("Cloud/SyncEnabled", true).toBool();
    biz_type_ = settings.value("AI/BizType", "PEDESTRIAN").toString();
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

    settings.setValue("Alert/CrowdSurgeEnabled", crowd_surge_enabled_);
    settings.setValue("Alert/CrowdSurgeThreshold", crowd_surge_threshold_);
    settings.setValue("Alert/CrowdSurgeBeep", crowd_surge_beep_);

    settings.setValue("Webhook/Enabled", webhook_enabled_);
    settings.setValue("Webhook/Url", webhook_url_);
    settings.setValue("Webhook/Token", webhook_token_);
    settings.setValue("Webhook/TimeoutMs", webhook_timeout_ms_);

    settings.setValue("Cloud/ServerUrl", cloud_server_url_);
    settings.setValue("Cloud/ClientName", cloud_client_name_);
    settings.setValue("Cloud/UserPhone", cloud_user_phone_);
    settings.setValue("Cloud/CompanyName", cloud_company_name_);
    settings.setValue("Cloud/PointId", cloud_point_id_);
    settings.setValue("Cloud/PointName", cloud_point_name_);
    settings.setValue("Cloud/PointCode", cloud_point_code_);
    settings.setValue("Cloud/DevicePassword", cloud_device_password_);
    settings.setValue("Cloud/PointToken", cloud_point_token_);
    settings.setValue("Cloud/SyncEnabled", cloud_sync_enabled_);
    settings.setValue("AI/BizType", biz_type_);
    settings.sync();

    emit config_changed();
}
