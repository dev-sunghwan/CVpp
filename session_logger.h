#pragma once

#include <fstream>
#include <string>

class SessionLogger {
public:
    bool initialize(const std::string& output_root, std::string& error_message);
    void log_event(const std::string& message);
    void log_raw_metadata(const std::string& payload);

    const std::string& session_dir() const { return session_dir_; }
    const std::string& raw_metadata_path() const { return raw_metadata_path_; }

private:
    std::string session_dir_;
    std::string raw_metadata_path_;
    std::ofstream session_log_;
    std::ofstream raw_metadata_log_;
};
