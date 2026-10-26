package com.flowsight.cloud.service;

import com.baomidou.mybatisplus.extension.plugins.pagination.Page;
import com.flowsight.cloud.dto.DeviceLoginDTO;
import com.flowsight.cloud.dto.PairingRequestDTO;
import com.flowsight.cloud.dto.PointCreateDTO;
import com.flowsight.cloud.dto.PointUpdateDTO;
import com.flowsight.cloud.dto.ResetDevicePasswordDTO;
import com.flowsight.cloud.entity.BizPoint;
import com.flowsight.cloud.vo.DeviceLoginVO;
import com.flowsight.cloud.vo.PairingCodeVO;
import com.flowsight.cloud.vo.PointCredentialVO;
import com.flowsight.cloud.vo.PointVO;

import java.util.List;

public interface PointService {
    Page<PointVO> pagePoints(int page, int size, Long userId, String keyword, String bizType);
    List<PointVO> listUserPoints(Long userId);
    PointVO createPoint(PointCreateDTO dto);
    void updatePoint(Long id, PointUpdateDTO dto);
    void deletePointCascadeCloud(Long id);
    PointVO getPointById(Long id);
    BizPoint getPointByToken(String token);
    String refreshToken(Long id);
    String generateUniquePointCode(String bizType);

    // 双密码体系接口
    DeviceLoginVO loginWithDeviceCredentials(DeviceLoginDTO dto);
    PointCredentialVO getPointCredential(Long pointId, Long userId);
    PointCredentialVO resetDevicePassword(Long pointId, Long userId, ResetDevicePasswordDTO dto);

    // 极简 6 位动态配对码接口
    PairingCodeVO generatePairingCode(Long pointId, Long userId);
    DeviceLoginVO pairWithCode(PairingRequestDTO dto);

    // 点位独占绑定与主动解绑互斥接口
    DeviceLoginVO bindPoint(Long pointId, String clientName, boolean force);
    void unbindPoint(Long pointId, String clientName);
}
