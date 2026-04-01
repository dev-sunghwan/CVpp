#pragma once

#include <string>
#include <vector>

#include "metadata_types.h"

struct AppConfig;

struct SessionTypeMetricRow {
    std::string normalized_type;
    int detection_count = 0;
    int unique_object_count = 0;
};

struct SessionSummaryRow {
    int raw_samples = 0;
    int parsed_payloads = 0;
    int malformed_payloads = 0;
    int event_only_payloads = 0;
    int detection_events = 0;
    std::vector<SessionTypeMetricRow> type_metrics;
};

class SqliteSessionStore {
public:
    SqliteSessionStore();
    ~SqliteSessionStore();

    bool initialize(const std::string& output_root,
                    const std::string& session_dir,
                    const std::string& session_log_path,
                    const std::string& raw_metadata_path,
                    const std::string& parsed_summary_path,
                    const AppConfig& config,
                    const std::string& started_at,
                    std::string& error_message);

    bool append_parsed_payload(const std::string& observed_at,
                               const std::string& parse_status,
                               const std::string& parse_message,
                               const std::vector<DetectedObject>& objects,
                               bool has_video_analytics,
                               std::string& error_message);

    bool finalize_session(const std::string& ended_at,
                          const SessionSummaryRow& summary,
                          std::string& error_message);

    const std::string& db_path() const { return db_path_; }

private:
    void close();
    bool execute_sql(const char* sql, std::string& error_message);
    bool prepare_schema(std::string& error_message);
    bool insert_session_artifacts(const std::string& session_log_path,
                                  const std::string& raw_metadata_path,
                                  const std::string& parsed_summary_path,
                                  std::string& error_message);
    bool parse_rtsp_identity(const std::string& rtsp_url,
                             std::string& camera_host,
                             std::string& profile_name) const;

    struct sqlite3* db_ = nullptr;
    std::string db_path_;
    std::string session_id_;
};
