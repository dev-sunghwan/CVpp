#include "session_logger.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>

#include "app_config.h"
#include "shared_app_state.h"
#include "sqlite_session_store.h"

namespace {
std::string timestamp_for_path() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time_value = std::chrono::system_clock::to_time_t(now);
    std::tm local_time{};
#ifdef _WIN32
    localtime_s(&local_time, &time_value);
#else
    localtime_r(&local_time, &time_value);
#endif
    std::ostringstream stream;
    stream << std::put_time(&local_time, "%Y%m%d-%H%M%S");
    return stream.str();
}

std::string timestamp_for_log() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time_value = std::chrono::system_clock::to_time_t(now);
    std::tm local_time{};
#ifdef _WIN32
    localtime_s(&local_time, &time_value);
#else
    localtime_r(&local_time, &time_value);
#endif
    std::ostringstream stream;
    stream << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S");
    return stream.str();
}

std::string format_type_counts(const std::map<std::string, int>& counts) {
    if (counts.empty()) {
        return "none";
    }

    std::ostringstream text;
    bool first = true;
    for (const auto& entry : counts) {
        if (!first) {
            text << ", ";
        }
        first = false;
        text << entry.first << "=" << entry.second;
    }
    return text.str();
}
}

std::string SessionLogger::current_log_timestamp() const {
    return timestamp_for_log();
}

void SessionLogger::log_sqlite_error_once(const std::string& prefix, const std::string& error_message) {
    if (sqlite_error_logged_) {
        return;
    }
    sqlite_error_logged_ = true;
    log_event(prefix + error_message);
}

bool SessionLogger::initialize(const AppConfig& config, std::string& error_message) {
    try {
        const std::filesystem::path working_dir = std::filesystem::current_path();
        const std::filesystem::path configured_output_root(config.output_root);
        const std::filesystem::path resolved_output_root = configured_output_root.is_absolute()
            ? configured_output_root
            : (working_dir / configured_output_root);
        std::filesystem::path session_path = resolved_output_root / ("session-" + timestamp_for_path());
        std::filesystem::create_directories(session_path);

        session_dir_ = session_path.string();
        session_log_path_ = (session_path / "session.log").string();
        raw_metadata_path_ = (session_path / "metadata_raw.xml.log").string();
        parsed_summary_path_ = (session_path / "parsed_summary.log").string();

        session_log_.open(session_log_path_, std::ios::out | std::ios::app);
        raw_metadata_log_.open(raw_metadata_path_, std::ios::out | std::ios::app);
        parsed_summary_log_.open(parsed_summary_path_, std::ios::out | std::ios::app);
    } catch (const std::exception& ex) {
        error_message = ex.what();
        return false;
    }

    if (!session_log_.is_open() || !raw_metadata_log_.is_open() || !parsed_summary_log_.is_open()) {
        error_message = "Failed to open session log files.";
        return false;
    }

    log_event("Session started");
    log_event(std::string("Session working directory: ") + std::filesystem::current_path().string());
    log_event(std::string("Session output directory: ") + session_dir_);

    sqlite_store_ = std::make_unique<SqliteSessionStore>();
    std::string sqlite_error;
    if (!sqlite_store_->initialize(config.output_root,
                                   session_dir_,
                                   session_log_path_,
                                   raw_metadata_path_,
                                   parsed_summary_path_,
                                   config,
                                   current_log_timestamp(),
                                   sqlite_error)) {
        log_event(std::string("SQLite review store unavailable: ") + sqlite_error);
        sqlite_store_.reset();
    }

    error_message.clear();
    return true;
}

void SessionLogger::log_event(const std::string& message) {
    if (session_log_.is_open()) {
        session_log_ << "[" << current_log_timestamp() << "] " << message << std::endl;
    }
}

void SessionLogger::log_raw_metadata(const std::string& payload) {
    if (raw_metadata_log_.is_open()) {
        raw_metadata_log_ << "===== " << current_log_timestamp() << " =====" << std::endl;
        raw_metadata_log_ << payload << std::endl;
    }
}

void SessionLogger::log_parsed_summary(const std::string& summary) {
    if (parsed_summary_log_.is_open()) {
        parsed_summary_log_ << "[" << current_log_timestamp() << "] " << summary << std::endl;
    }
}

void SessionLogger::record_parsed_payload(const std::string& parse_status,
                                          const std::string& parse_message,
                                          const std::vector<DetectedObject>& objects,
                                          bool has_video_analytics) {
    if (!sqlite_store_) {
        return;
    }

    std::string sqlite_error;
    if (!sqlite_store_->append_parsed_payload(current_log_timestamp(),
                                              parse_status,
                                              parse_message,
                                              objects,
                                              has_video_analytics,
                                              sqlite_error)) {
        log_sqlite_error_once("SQLite parsed payload write failed: ", sqlite_error);
        sqlite_store_.reset();
    }
}

void SessionLogger::finalize_session(SharedAppState& state) {
    std::map<std::string, int> detections_by_type;
    std::map<std::string, int> unique_ids_by_type;
    SessionSummaryRow summary;

    {
        std::lock_guard<std::mutex> lock(state.meta_mutex);
        summary.raw_samples = state.total_raw_metadata_samples;
        summary.parsed_payloads = state.total_parsed_payloads;
        summary.malformed_payloads = state.total_malformed_payloads;
        summary.event_only_payloads = state.total_event_only_payloads;
        summary.detection_events = state.total_detection_events;

        for (const auto& entry : state.detections_by_type) {
            detections_by_type[entry.first] = entry.second;
        }
        for (const auto& entry : state.unique_ids_by_type) {
            unique_ids_by_type[entry.first] = static_cast<int>(entry.second.size());
        }
    }

    std::ostringstream headline;
    headline << "Session metrics: raw_samples=" << summary.raw_samples
             << " parsed_payloads=" << summary.parsed_payloads
             << " malformed=" << summary.malformed_payloads
             << " event_only=" << summary.event_only_payloads
             << " detection_events=" << summary.detection_events;
    log_event(headline.str());
    log_event(std::string("Session metrics: detections_by_type=") + format_type_counts(detections_by_type));
    log_event(std::string("Session metrics: unique_ids_by_type=") + format_type_counts(unique_ids_by_type));

    for (const auto& entry : detections_by_type) {
        SessionTypeMetricRow row;
        row.normalized_type = entry.first;
        row.detection_count = entry.second;
        auto unique_it = unique_ids_by_type.find(entry.first);
        row.unique_object_count = unique_it != unique_ids_by_type.end() ? unique_it->second : 0;
        summary.type_metrics.push_back(row);
    }
    for (const auto& entry : unique_ids_by_type) {
        if (detections_by_type.count(entry.first)) {
            continue;
        }
        SessionTypeMetricRow row;
        row.normalized_type = entry.first;
        row.unique_object_count = entry.second;
        summary.type_metrics.push_back(row);
    }

    if (!sqlite_store_) {
        return;
    }

    std::string sqlite_error;
    if (!sqlite_store_->finalize_session(current_log_timestamp(), summary, sqlite_error)) {
        log_sqlite_error_once("SQLite session finalization failed: ", sqlite_error);
        sqlite_store_.reset();
    }
}

bool SessionLogger::capture_fixture_candidate(const std::string& output_dir, int limit, const std::string& payload, std::string& saved_path) {
    saved_path.clear();
    if (limit <= 0 || fixture_samples_written_ >= limit) {
        return false;
    }

    try {
        std::filesystem::path fixture_dir(output_dir);
        std::filesystem::create_directories(fixture_dir);

        std::ostringstream file_name;
        file_name << "sample-" << std::setw(2) << std::setfill('0') << (fixture_samples_written_ + 1) << ".xml";
        std::filesystem::path file_path = fixture_dir / file_name.str();

        std::ofstream fixture_file(file_path.string(), std::ios::out | std::ios::trunc);
        fixture_file << payload;
        fixture_file.close();

        if (!fixture_file) {
            return false;
        }

        ++fixture_samples_written_;
        saved_path = file_path.string();
        return true;
    } catch (...) {
        return false;
    }
}

