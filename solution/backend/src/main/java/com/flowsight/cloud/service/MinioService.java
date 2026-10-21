package com.flowsight.cloud.service;

import org.springframework.web.multipart.MultipartFile;

public interface MinioService {
    void ensureBucketExists();
    String uploadClip(Long pointId, Integer trackId, Long timestamp, MultipartFile file);
    String getPresignedPlayUrl(String clipPath);
    void deletePointClips(Long pointId);
}
