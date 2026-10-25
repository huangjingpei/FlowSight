package com.flowsight.cloud.dto;

import com.fasterxml.jackson.annotation.JsonAlias;
import com.fasterxml.jackson.annotation.JsonProperty;
import lombok.Data;

@Data
public class EdgeHeartbeatDTO {
    @JsonProperty("client_name")
    @JsonAlias({"clientName", "client_name"})
    private String clientName;

    @JsonProperty("client_version")
    @JsonAlias({"clientVersion", "client_version"})
    private String clientVersion;

    private Float fps;

    @JsonProperty("infer_latency_ms")
    @JsonAlias({"inferLatencyMs", "infer_latency_ms"})
    private Float inferLatencyMs;

    @JsonProperty("hardware_backend")
    @JsonAlias({"hardwareBackend", "hardware_backend"})
    private String hardwareBackend;

    @JsonProperty("current_occupancy")
    @JsonAlias({"currentOccupancy", "current_occupancy"})
    private Integer currentOccupancy;

    @JsonProperty("status")
    @JsonAlias({"status", "streamStatus", "stream_status"})
    private String status;
}
