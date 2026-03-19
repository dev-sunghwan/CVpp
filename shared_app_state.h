#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

#include <opencv2/opencv.hpp>

#include "metadata_types.h"

struct SharedAppState {
    cv::Mat current_frame;
    std::vector<DetectedObject> current_objects;
    std::mutex frame_mutex;
    std::mutex meta_mutex;
    bool new_frame_available = false;
    bool has_metadata_update = false;
    std::chrono::steady_clock::time_point last_metadata_update{};
    std::string last_parse_status_text = "No metadata parsed yet";
};
