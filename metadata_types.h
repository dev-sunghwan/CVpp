#pragma once

#include <string>

struct DetectedObject {
    int id = 0;
    std::string type = "Unknown";
    float likelihood = 0.0f;
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
};
