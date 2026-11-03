package com.flowsight.cloud.vo;

import lombok.AllArgsConstructor;
import lombok.Builder;
import lombok.Data;
import lombok.NoArgsConstructor;

@Data
@Builder
@NoArgsConstructor
@AllArgsConstructor
public class SystemFeaturesVO {
    /**
     * 视频推流播放与证据切片功能开关 (默认关闭 false)
     */
    private boolean videoPlaybackEnabled;
}
