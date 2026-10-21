package com.flowsight.cloud.controller;

import com.flowsight.cloud.common.Result;
import com.flowsight.cloud.dto.LoginDTO;
import com.flowsight.cloud.service.AuthService;
import com.flowsight.cloud.vo.LoginVO;
import com.flowsight.cloud.vo.QrCodeVO;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.validation.Valid;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.web.bind.annotation.*;

@RestController
@RequestMapping("/api/v1/auth")
public class AuthController {

    @Autowired
    private AuthService authService;

    @PostMapping("/login")
    public Result<LoginVO> login(@Valid @RequestBody LoginDTO dto) {
        return Result.success(authService.login(dto));
    }

    @PostMapping("/qrcode/generate")
    public Result<QrCodeVO> generateQrCode() {
        return Result.success(authService.generateQrCode());
    }

    @PostMapping("/qrcode/scan")
    public Result<Void> scanQrCode(@RequestParam String ticket, HttpServletRequest request) {
        Long userId = (Long) request.getAttribute("userId");
        authService.scanQrCode(ticket, userId);
        return Result.success(null);
    }

    @PostMapping("/qrcode/confirm")
    public Result<Void> confirmQrCode(@RequestParam String ticket,
                                      @RequestParam(required = false) Long pointId,
                                      HttpServletRequest request) {
        Long userId = (Long) request.getAttribute("userId");
        authService.confirmQrCode(ticket, userId, pointId);
        return Result.success(null);
    }

    @GetMapping("/qrcode/poll")
    public Result<QrCodeVO> pollQrCode(@RequestParam String ticket) {
        return Result.success(authService.pollQrCode(ticket));
    }
}
