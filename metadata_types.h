#pragma once

#include <string>
#include <vector>

enum class ParseStatus {
    Success,
    UnknownPattern,
    NoObjects,
    MalformedPayload
};

struct DetectedObject {
    int id = 0;
    std::string type = "Unknown";
    float likelihood = 0.0f;
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
};

struct MetadataParseResult {
    ParseStatus status = ParseStatus::MalformedPayload;
    std::string message;
    std::vector<DetectedObject> objects;
};
