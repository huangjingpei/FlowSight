#pragma once
#include "common/Types.hpp"
#include <vector>

class ITracker {
public:
    virtual ~ITracker() = default;

    // 输入当前帧检测框，更新卡尔曼滤波与匈牙利匹配，输出活跃轨迹
    virtual std::vector<TrackObject> update(const std::vector<Detection>& detections) = 0;

    // 重置所有内部轨迹状态
    virtual void reset() = 0;
};
