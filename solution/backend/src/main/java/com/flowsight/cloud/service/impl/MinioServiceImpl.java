package com.flowsight.cloud.service.impl;

import com.flowsight.cloud.service.MinioService;
import io.minio.*;
import io.minio.http.Method;
import io.minio.messages.DeleteError;
import io.minio.messages.DeleteObject;
import io.minio.messages.Item;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;
import org.springframework.web.multipart.MultipartFile;

import java.io.InputStream;
import java.time.Instant;
import java.time.LocalDate;
import java.time.ZoneId;
import java.time.format.DateTimeFormatter;
import java.util.LinkedList;
import java.util.List;
import java.util.concurrent.TimeUnit;

@Service
public class MinioServiceImpl implements MinioService {
    private static final Logger log = LoggerFactory.getLogger(MinioServiceImpl.class);

    @Autowired
    private MinioClient minioClient;

    @Value("${minio.bucket-name}")
    private String bucketName;

    @Override
    public void ensureBucketExists() {
        try {
            boolean exists = minioClient.bucketExists(BucketExistsArgs.builder().bucket(bucketName).build());
            if (!exists) {
                minioClient.makeBucket(MakeBucketArgs.builder().bucket(bucketName).build());
                log.info("[MinIO] Bucket created: {}", bucketName);
            }
        } catch (Exception e) {
            log.warn("[MinIO] Check or create bucket warning: {}", e.getMessage());
        }
    }

    @Override
    public String uploadClip(Long pointId, Integer trackId, Long timestamp, MultipartFile file) {
        try {
            ensureBucketExists();
            LocalDate date = Instant.ofEpochMilli(timestamp != null ? timestamp : System.currentTimeMillis())
                    .atZone(ZoneId.systemDefault()).toLocalDate();
            String dateStr = date.format(DateTimeFormatter.ofPattern("yyyyMMdd"));
            
            String objectName = String.format("clips/%d/%s/track_%d_%d.mp4", 
                    pointId, dateStr, trackId != null ? trackId : 0, timestamp != null ? timestamp : System.currentTimeMillis());

            try (InputStream is = file.getInputStream()) {
                minioClient.putObject(
                        PutObjectArgs.builder()
                                .bucket(bucketName)
                                .object(objectName)
                                .stream(is, file.getSize(), -1)
                                .contentType("video/mp4")
                                .build()
                );
            }
            log.info("[MinIO] Uploaded clip: {}", objectName);
            return objectName;
        } catch (Exception e) {
            log.error("[MinIO] Failed to upload clip", e);
            throw new RuntimeException("上传切片至 MinIO 失败: " + e.getMessage());
        }
    }

    @Override
    public String getPresignedPlayUrl(String clipPath) {
        if (clipPath == null || clipPath.trim().isEmpty()) {
            return "";
        }
        try {
            return minioClient.getPresignedObjectUrl(
                    GetPresignedObjectUrlArgs.builder()
                            .method(Method.GET)
                            .bucket(bucketName)
                            .object(clipPath)
                            .expiry(15, TimeUnit.MINUTES)
                            .build()
            );
        } catch (Exception e) {
            log.warn("[MinIO] Failed to generate presigned URL for {}: {}", clipPath, e.getMessage());
            return "";
        }
    }

    @Override
    public void deletePointClips(Long pointId) {
        try {
            String prefix = String.format("clips/%d/", pointId);
            Iterable<io.minio.Result<Item>> results = minioClient.listObjects(
                    ListObjectsArgs.builder().bucket(bucketName).prefix(prefix).recursive(true).build()
            );
            List<DeleteObject> objects = new LinkedList<>();
            for (io.minio.Result<Item> r : results) {
                Item item = r.get();
                objects.add(new DeleteObject(item.objectName()));
            }
            if (!objects.isEmpty()) {
                Iterable<io.minio.Result<DeleteError>> errors = minioClient.removeObjects(
                        RemoveObjectsArgs.builder().bucket(bucketName).objects(objects).build()
                );
                for (io.minio.Result<DeleteError> errorResult : errors) {
                    DeleteError error = errorResult.get();
                    log.warn("[MinIO] Error deleting object {}: {}", error.objectName(), error.message());
                }
                log.info("[MinIO] Cleaned {} clips for point {}", objects.size(), pointId);
            }
        } catch (Exception e) {
            log.error("[MinIO] Failed to delete point clips for pointId: {}", pointId, e);
        }
    }
}
