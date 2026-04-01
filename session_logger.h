#pragma once

#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "metadata_types.h"
#include "sqlite_session_store.h"

struct AppConfig;
struct SharedAppState;

class SessionLogger {
public:
    bool initialize(const AppConfig& config, std::string& error_message);
    void log_event(const std::string& message);
    void log_raw_metadata(const std::string& payload);
    void log_parsed_summary(const std::string& summary);
    void record_parsed_payload(const std::string& parse_status,
                               const std::string& parse_message,
                               const std::vector<DetectedObject>& objects,
                               bool has_video_analytics);
    void finalize_session(SharedAppState& state);
    bool capture_fixture_candidate(const std::string& output_dir, int limit, const std::string& payload, std::string& saved_path);

    const std::string& session_dir() const { return session_dir_; }
    const std::string& session_log_path() const { return session_log_path_; }
    const std::string& raw_metadata_path() const { return raw_metadata_path_; }
    const std::string& parsed_summary_path() const { return parsed_summary_path_; }

private:
    std::string current_log_timestamp() const;
    void log_sqlite_error_once(const std::string& prefix, const std::string& error_message);

    std::string session_dir_;
    std::string session_log_path_;
    std::string raw_metadata_path_;
    std::string parsed_summary_path_;
    std::ofstream session_log_;
    std::ofstream raw_metadata_log_;
    std::ofstream parsed_summary_log_;
    int fixture_samples_written_ = 0;
    bool sqlite_error_logged_ = false;
    std::unique_ptr<SqliteSessionStore> sqlite_store_;
};
