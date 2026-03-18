#include "app_config.h"

#include <cctype>
#include <fstream>
#include <string>

namespace {
std::string trim(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }

    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return value.substr(start, end - start);
}

std::string unquote(const std::string& value) {
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

bool parse_bool(const std::string& value, bool& out) {
    if (value == "true") {
        out = true;
        return true;
    }
    if (value == "false") {
        out = false;
        return true;
    }
    return false;
}
}

bool load_config(const std::string& path, AppConfig& config, std::string& error_message) {
    std::ifstream input(path);
    if (!input.is_open()) {
        error_message = "Failed to open config file: " + path;
        return false;
    }

    AppConfig parsed;
    std::string section;
    std::string line;
    int line_number = 0;

    while (std::getline(input, line)) {
        ++line_number;
        std::string current = trim(line);
        if (current.empty() || current[0] == '#') {
            continue;
        }

        if (current.front() == '[' && current.back() == ']') {
            section = current.substr(1, current.size() - 2);
            continue;
        }

        size_t eq = current.find('=');
        if (eq == std::string::npos) {
            error_message = "Invalid config entry at line " + std::to_string(line_number);
            return false;
        }

        std::string key = trim(current.substr(0, eq));
        std::string raw_value = trim(current.substr(eq + 1));
        std::string value = unquote(raw_value);

        if (section == "rtsp") {
            if (key == "url") {
                parsed.rtsp_url = value;
            } else if (key == "latency") {
                try {
                    parsed.latency = std::stoi(value);
                } catch (...) {
                    error_message = "Invalid integer for rtsp.latency at line " + std::to_string(line_number);
                    return false;
                }
            }
        } else if (section == "headers") {
            parsed.headers[key] = value;
        } else if (section == "logging") {
            if (key == "output_root") {
                parsed.output_root = value;
            }
        } else if (section == "fixtures") {
            if (key == "capture_samples") {
                if (!parse_bool(value, parsed.capture_fixture_candidates)) {
                    error_message = "Invalid boolean for fixtures.capture_samples at line " + std::to_string(line_number);
                    return false;
                }
            } else if (key == "sample_limit") {
                try {
                    parsed.fixture_sample_limit = std::stoi(value);
                } catch (...) {
                    error_message = "Invalid integer for fixtures.sample_limit at line " + std::to_string(line_number);
                    return false;
                }
            } else if (key == "output_dir") {
                parsed.fixture_output_dir = value;
            }
        }
    }

    if (parsed.rtsp_url.empty()) {
        error_message = "Config is missing rtsp.url";
        return false;
    }

    config = parsed;
    return true;
}
