#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <opencv2/opencv.hpp>

#include "metadata_types.h"

struct ParserHealthCounts {
    int clean_object_payloads = 0;
    int recovered_continuations = 0;
    int fragmented_object_payloads = 0;
    int continuation_chunks = 0;
    int metadata_without_objects = 0;
    int unknown_object_patterns = 0;
};

struct SharedAppState {
    cv::Mat current_frame;
    std::vector<DetectedObject> current_objects;
    std::mutex frame_mutex;
    std::mutex meta_mutex;
    bool new_frame_available = false;
    bool has_metadata_update = false;
    std::chrono::steady_clock::time_point last_metadata_update{};
    std::chrono::steady_clock::time_point last_raw_metadata_seen{};
    std::string last_parse_status_text = "No metadata parsed yet";
    int last_parsed_object_count = 0;
    int total_raw_metadata_samples = 0;
    int total_parsed_payloads = 0;
    int total_malformed_payloads = 0;
    int total_event_only_payloads = 0;
    int total_detection_events = 0;
    std::unordered_map<std::string, int> detections_by_type;
    std::unordered_map<std::string, std::unordered_set<int>> unique_ids_by_type;
    ParserHealthCounts parser_health_counts;
    std::vector<std::string> recent_parsed_summaries;
};

