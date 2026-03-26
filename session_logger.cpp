#include "session_logger.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

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
}

bool SessionLogger::initialize(const std::string& output_root, std::string& error_message) {
    try {
        const std::filesystem::path working_dir = std::filesystem::current_path();
        const std::filesystem::path configured_output_root(output_root);
        const std::filesystem::path resolved_output_root = configured_output_root.is_absolute()
            ? configured_output_root
            : (working_dir / configured_output_root);
        std::filesystem::path session_path = resolved_output_root / ("session-" + timestamp_for_path());
        std::filesystem::create_directories(session_path);

        session_dir_ = session_path.string();
        raw_metadata_path_ = (session_path / "metadata_raw.xml.log").string();
        parsed_summary_path_ = (session_path / "parsed_summary.log").string();

        session_log_.open((session_path / "session.log").string(), std::ios::out | std::ios::app);
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
    return true;
}

void SessionLogger::log_event(const std::string& message) {
    if (session_log_.is_open()) {
        session_log_ << "[" << timestamp_for_log() << "] " << message << std::endl;
    }
}

void SessionLogger::log_raw_metadata(const std::string& payload) {
    if (raw_metadata_log_.is_open()) {
        raw_metadata_log_ << "===== " << timestamp_for_log() << " =====" << std::endl;
        raw_metadata_log_ << payload << std::endl;
    }
}

void SessionLogger::log_parsed_summary(const std::string& summary) {
    if (parsed_summary_log_.is_open()) {
        parsed_summary_log_ << "[" << timestamp_for_log() << "] " << summary << std::endl;
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

