package com.flowsight.cloud.dto;

import com.fasterxml.jackson.annotation.JsonProperty;
import lombok.Data;

import java.util.Map;

@Data
public class EdgeEventDTO {
    @JsonProperty("event_type")
    private String eventType;

    private String direction; // ENTRY, EXIT

    @JsonProperty("track_id")
    private Integer trackId;

    private Long timestamp;

    private String datetime;

    private Float confidence;

    @JsonProperty("source_id")
    private String sourceId;

    @JsonProperty("clip_path")
    private String clipPath;

    private Map<String, String> attributes; // gender, age_group
}
