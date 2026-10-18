#pragma once

#include <QObject>
#include <QString>
#include <QSettings>

class ConfigManager : public QObject {
    Q_OBJECT
public:
    static ConfigManager& instance();

    void load(const QString& ini_path = "configs/flowsight.ini");
    void save();

    QString get_video_source() const { return video_source_; }
    void set_video_source(const QString& val) { video_source_ = val; }

    int get_clip_retention_days() const { return clip_retention_days_; }
    void set_clip_retention_days(int days) { clip_retention_days_ = days; }

    float get_conf_threshold() const { return conf_thresh_; }
    void set_conf_threshold(float val) { conf_thresh_ = val; }

    float get_nms_threshold() const { return nms_thresh_; }
    void set_nms_threshold(float val) { nms_thresh_ = val; }

    float get_buffer_distance() const { return buffer_dist_; }
    void set_buffer_distance(float val) { buffer_dist_ = val; }

    int get_confirm_frames() const { return confirm_frames_; }
    void set_confirm_frames(int val) { confirm_frames_ = val; }

    int get_audio_volume() const { return audio_volume_; }
    void set_audio_volume(int val) { audio_volume_ = val; }

    bool get_crowd_surge_enabled() const { return crowd_surge_enabled_; }
    void set_crowd_surge_enabled(bool val) { crowd_surge_enabled_ = val; }

    int get_crowd_surge_threshold() const { return crowd_surge_threshold_; }
    void set_crowd_surge_threshold(int val) { crowd_surge_threshold_ = val; }

    bool get_crowd_surge_beep() const { return crowd_surge_beep_; }
    void set_crowd_surge_beep(bool val) { crowd_surge_beep_ = val; }

signals:
    void config_changed();

private:
    ConfigManager();
    ~ConfigManager() override = default;

    QString ini_path_ = "configs/flowsight.ini";
    QString video_source_ = "configs/test_corridor.mp4";
    int clip_retention_days_ = 30;
    float conf_thresh_ = 0.45f;
    float nms_thresh_ = 0.50f;
    float buffer_dist_ = 20.0f;
    int confirm_frames_ = 4;
    int audio_volume_ = 80;

    bool crowd_surge_enabled_ = true;
    int crowd_surge_threshold_ = 10;
    bool crowd_surge_beep_ = false;
};
