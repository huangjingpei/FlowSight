package com.flowsight.cloud.dto;

import com.fasterxml.jackson.annotation.JsonProperty;
import lombok.Data;

@Data
public class EdgeHeartbeatDTO {
    @JsonProperty("client_version")
    private String clientVersion;

    private Float fps;

    @JsonProperty("infer_latency_ms")
    private Float inferLatencyMs;

    @JsonProperty("hardware_backend")
    private String hardwareBackend;

    @JsonProperty("current_occupancy")
    private Integer currentOccupancy;
}
