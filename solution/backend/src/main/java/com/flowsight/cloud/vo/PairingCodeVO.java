package com.flowsight.cloud.vo;

import lombok.AllArgsConstructor;
import lombok.Builder;
import lombok.Data;
import lombok.NoArgsConstructor;

@Data
@Builder
@NoArgsConstructor
@AllArgsConstructor
public class PairingCodeVO {
    private String pairingCode;
    private Long pointId;
    private String pointName;
    private Integer expiresInSeconds;
}
