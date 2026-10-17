#pragma once
#include "common/Types.hpp"
#include <memory>
#include <vector>

class RoiCounter {
public:
    RoiCounter();
    ~RoiCounter();

    void set_roi_config(std::shared_ptr<const RoiConfig> config);

    void process_frame(const std::vector<TrackObject>& tracks,
                       int64_t timestamp_ms,
                       int& current_occupancy,
                       std::vector<PassageEvent>& out_events);

    void reset();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
