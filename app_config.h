#pragma once

#include <map>
#include <string>

struct AppConfig {
    std::string rtsp_url;
    int latency = 100;
    std::map<std::string, std::string> headers;
    std::string output_root = "output";
};

bool load_config(const std::string& path, AppConfig& config, std::string& error_message);
