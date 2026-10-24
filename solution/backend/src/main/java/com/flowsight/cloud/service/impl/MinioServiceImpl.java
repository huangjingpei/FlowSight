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

import java.io.File;
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
        LocalDate date = Instant.ofEpochMilli(timestamp != null ? timestamp : System.currentTimeMillis())
                .atZone(ZoneId.systemDefault()).toLocalDate();
        String dateStr = date.format(DateTimeFormatter.ofPattern("yyyyMMdd"));
        
        String objectName = String.format("clips/%d/%s/track_%d_%d.mp4", 
                pointId, dateStr, trackId != null ? trackId : 0, timestamp != null ? timestamp : System.currentTimeMillis());

        // 1. 尝试上传至 MinIO 对象存储
        try {
            ensureBucketExists();
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
            log.info("[MinIO] Uploaded clip to MinIO: {}", objectName);
            return objectName;
        } catch (Exception e) {
            log.warn("[MinIO] MinIO unavailable or upload failed ({}), falling back to local disk storage", e.getMessage());
            // 2. 本地磁盘降级容灾存储 (无缝支持单机开发、离线局域网或 MinIO 未启动环境)
            try {
                File targetFile = new File("uploads", objectName);
                File parent = targetFile.getParentFile();
                if (parent != null && !parent.exists()) {
                    parent.mkdirs();
                }
                file.transferTo(targetFile.toPath());
                log.info("[StorageFallback] Clip successfully stored locally: {}", targetFile.getAbsolutePath());
                return objectName;
            } catch (Exception ex) {
                log.error("[StorageFallback] Failed to store clip locally", ex);
                throw new RuntimeException("切片存储失败 (MinIO及本地存储均不可用): " + ex.getMessage());
            }
        }
    }

    @Override
    public String getPresignedPlayUrl(String clipPath) {
        if (clipPath == null || clipPath.trim().isEmpty()) {
            return "";
        }

        // 1. 若本地磁盘已存在该切片 (本地开发模式或降级存储)，优先直接返回后端静态映射直链
        File localFile = new File("uploads", clipPath);
        if (localFile.exists()) {
            String relative = clipPath.startsWith("clips/") ? clipPath.substring(6) : clipPath;
            return "/api/v1/clips/" + relative;
        }

        // 2. 尝试从 MinIO 获取预签名直链
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
            log.warn("[MinIO] Failed to generate presigned URL for {}: {}, fallback to local static route", clipPath, e.getMessage());
            String relative = clipPath.startsWith("clips/") ? clipPath.substring(6) : clipPath;
            return "/api/v1/clips/" + relative;
        }
    }

    @Override
    public void deletePointClips(Long pointId) {
        // 1. 删除 MinIO 中的切片
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
                minioClient.removeObjects(
                        RemoveObjectsArgs.builder().bucket(bucketName).objects(objects).build()
                );
                log.info("[MinIO] Cleaned {} clips for point {}", objects.size(), pointId);
            }
        } catch (Exception e) {
            log.warn("[MinIO] Failed to delete point clips from MinIO for pointId: {}", pointId);
        }

        // 2. 删除本地磁盘降级目录中的切片
        try {
            File pointDir = new File("uploads/clips/" + pointId);
            if (pointDir.exists()) {
                deleteDirectoryRecursively(pointDir);
                log.info("[StorageFallback] Deleted local clip dir for point {}", pointId);
            }
        } catch (Exception e) {
            log.warn("[StorageFallback] Failed to delete local clips for pointId: {}", pointId, e);
        }
    }

    private void deleteDirectoryRecursively(File file) {
        if (file.isDirectory()) {
            File[] files = file.listFiles();
            if (files != null) {
                for (File child : files) {
                    deleteDirectoryRecursively(child);
                }
            }
        }
        file.delete();
    }
}
