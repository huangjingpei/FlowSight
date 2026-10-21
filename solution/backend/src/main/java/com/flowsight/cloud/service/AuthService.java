package com.flowsight.cloud.service;

import com.flowsight.cloud.dto.LoginDTO;
import com.flowsight.cloud.vo.LoginVO;
import com.flowsight.cloud.vo.QrCodeVO;

public interface AuthService {
    LoginVO login(LoginDTO dto);
    QrCodeVO generateQrCode();
    void scanQrCode(String ticket, Long userId);
    void confirmQrCode(String ticket, Long userId, Long pointId);
    QrCodeVO pollQrCode(String ticket);
}
