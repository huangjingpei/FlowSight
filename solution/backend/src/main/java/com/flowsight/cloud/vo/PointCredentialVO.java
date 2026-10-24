package com.flowsight.cloud.vo;

import lombok.AllArgsConstructor;
import lombok.Builder;
import lombok.Data;
import lombok.NoArgsConstructor;

@Data
@Builder
@NoArgsConstructor
@AllArgsConstructor
public class PointCredentialVO {

    private Long pointId;
    private String pointName;
    private String pointCode;
    private String pointToken;
    private Boolean hasDevicePassword;
    private String devicePasswordMasked;
}
