#pragma once
#include <QObject>
#include <QThread>
#include <QImage>
#include <memory>
#include <string>
#include "common/Types.hpp"

class PipelineController : public QObject {
    Q_OBJECT
public:
    explicit PipelineController(QObject* parent = nullptr);
    ~PipelineController() override;

    void start_pipeline(const std::string& model_path, const std::string& video_source);
    void stop_pipeline();
    void update_roi_config(const RoiConfig& config);
    RoiConfig get_current_roi() const;

signals:
    void frame_ready(const QImage& frame, const std::vector<TrackObject>& tracks);
    void stats_updated(const StatisticsSnapshot& stats);
    void event_occurred(const PassageEvent& event);

private:
    class Worker;
    std::unique_ptr<Worker> worker_;
    QThread worker_thread_;
};
