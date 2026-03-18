#pragma once

#include <fstream>
#include <string>

class SessionLogger {
public:
    bool initialize(const std::string& output_root, std::string& error_message);
    void log_event(const std::string& message);
    void log_raw_metadata(const std::string& payload);
    void log_parsed_summary(const std::string& summary);
    bool capture_fixture_candidate(const std::string& output_dir, int limit, const std::string& payload, std::string& saved_path);

    const std::string& session_dir() const { return session_dir_; }
    const std::string& raw_metadata_path() const { return raw_metadata_path_; }
    const std::string& parsed_summary_path() const { return parsed_summary_path_; }

private:
    std::string session_dir_;
    std::string raw_metadata_path_;
    std::string parsed_summary_path_;
    std::ofstream session_log_;
    std::ofstream raw_metadata_log_;
    std::ofstream parsed_summary_log_;
    int fixture_samples_written_ = 0;
};
