package com.flowsight.cloud.dto;

import com.fasterxml.jackson.annotation.JsonAlias;
import com.fasterxml.jackson.annotation.JsonProperty;
import lombok.Data;

import java.util.Map;

@Data
public class EdgeEventDTO {
    @JsonAlias({"event_type", "eventType"})
    @JsonProperty("eventType")
    private String eventType;

    private String direction; // ENTRY, EXIT

    @JsonAlias({"track_id", "trackId"})
    @JsonProperty("trackId")
    private Integer trackId;

    private Long timestamp;

    private String datetime;

    private Float confidence;

    @JsonAlias({"source_id", "sourceId"})
    @JsonProperty("sourceId")
    private String sourceId;

    @JsonAlias({"clip_path", "clipPath"})
    @JsonProperty("clipPath")
    private String clipPath;

    private Map<String, String> attributes; // gender, age_group
}
