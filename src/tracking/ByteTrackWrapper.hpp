#pragma once
#include "ITracker.hpp"
#include <memory>

class ByteTrackWrapper : public ITracker {
public:
    ByteTrackWrapper(int frame_rate = 30, int track_buffer = 60);
    ~ByteTrackWrapper() override;

    std::vector<TrackObject> update(const std::vector<Detection>& detections) override;
    void reset() override;
    void set_vehicle_mode(bool enable) override;
    bool is_vehicle_mode() const override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
