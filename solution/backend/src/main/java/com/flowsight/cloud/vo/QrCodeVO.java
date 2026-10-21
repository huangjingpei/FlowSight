package com.flowsight.cloud.vo;

import lombok.Data;

@Data
public class QrCodeVO {
    private String ticket;
    private String qrcodeContent;
    private String status; // WAIT_SCAN, SCANNED, CONFIRMED, EXPIRED
    private String token;  // 确认后下发的 JWT
}
