package com.flowsight.cloud;

import org.mybatis.spring.annotation.MapperScan;
import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.scheduling.annotation.EnableScheduling;

@SpringBootApplication
@EnableScheduling
@MapperScan("com.flowsight.cloud.mapper")
public class FlowSightCloudApplication {
    public static void main(String[] args) {
        SpringApplication.run(FlowSightCloudApplication.class, args);
    }
}
