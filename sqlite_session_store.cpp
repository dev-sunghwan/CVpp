#include "sqlite_session_store.h"

#include <filesystem>
#include <sstream>

#include <sqlite3.h>

#include "app_config.h"

namespace {
constexpr const char* kSchemaSql = R"SQL(
CREATE TABLE IF NOT EXISTS sessions (
    session_id TEXT PRIMARY KEY,
    started_at TEXT NOT NULL,
    ended_at TEXT,
    output_dir TEXT NOT NULL,
    camera_host TEXT,
    profile_name TEXT,
    metadata_enabled INTEGER NOT NULL,
    raw_samples INTEGER NOT NULL DEFAULT 0,
    parsed_payloads INTEGER NOT NULL DEFAULT 0,
    malformed_payloads INTEGER NOT NULL DEFAULT 0,
    event_only_payloads INTEGER NOT NULL DEFAULT 0,
    detection_events INTEGER NOT NULL DEFAULT 0,
    notes TEXT
);
CREATE TABLE IF NOT EXISTS session_artifacts (
    session_id TEXT NOT NULL,
    artifact_type TEXT NOT NULL,
    artifact_path TEXT NOT NULL,
    PRIMARY KEY(session_id, artifact_type)
);
CREATE TABLE IF NOT EXISTS parsed_payloads (
    payload_id INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id TEXT NOT NULL,
    observed_at TEXT NOT NULL,
    parse_status TEXT NOT NULL,
    parse_message TEXT NOT NULL,
    object_count INTEGER NOT NULL,
    has_video_analytics INTEGER NOT NULL,
    is_continuation_related INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS parsed_objects (
    parsed_object_id INTEGER PRIMARY KEY AUTOINCREMENT,
    payload_id INTEGER NOT NULL,
    session_id TEXT NOT NULL,
    object_id INTEGER NOT NULL,
    normalized_type TEXT NOT NULL,
    likelihood REAL NOT NULL,
    left_coord REAL NOT NULL,
    top_coord REAL NOT NULL,
    right_coord REAL NOT NULL,
    bottom_coord REAL NOT NULL
);
CREATE TABLE IF NOT EXISTS session_type_metrics (
    session_id TEXT NOT NULL,
    normalized_type TEXT NOT NULL,
    detection_count INTEGER NOT NULL,
    unique_object_count INTEGER NOT NULL,
    PRIMARY KEY(session_id, normalized_type)
);
)SQL";

bool bind_text(sqlite3_stmt* stmt, int index, const std::string& value) {
    return sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK;
}
}

SqliteSessionStore::SqliteSessionStore() = default;

SqliteSessionStore::~SqliteSessionStore() {
    close();
}

void SqliteSessionStore::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool SqliteSessionStore::execute_sql(const char* sql, std::string& error_message) {
    char* error = nullptr;
    const int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &error);
    if (rc != SQLITE_OK) {
        error_message = error ? error : "sqlite exec failed";
        if (error) {
            sqlite3_free(error);
        }
        return false;
    }
    return true;
}

bool SqliteSessionStore::prepare_schema(std::string& error_message) {
    return execute_sql(kSchemaSql, error_message);
}

bool SqliteSessionStore::parse_rtsp_identity(const std::string& rtsp_url,
                                             std::string& camera_host,
                                             std::string& profile_name) const {
    camera_host.clear();
    profile_name.clear();

    const std::string prefix = "rtsp://";
    if (rtsp_url.rfind(prefix, 0) != 0) {
        return false;
    }

    const std::string remainder = rtsp_url.substr(prefix.size());
    const size_t at_pos = remainder.find('@');
    const size_t slash_pos = remainder.find('/');
    if (slash_pos == std::string::npos) {
        return false;
    }

    if (at_pos != std::string::npos && at_pos < slash_pos) {
        camera_host = remainder.substr(at_pos + 1, slash_pos - at_pos - 1);
    } else {
        camera_host = remainder.substr(0, slash_pos);
    }

    const std::string path = remainder.substr(slash_pos + 1);
    const std::string profile_prefix = "profile";
    if (path.rfind(profile_prefix, 0) == 0) {
        size_t end = profile_prefix.size();
        while (end < path.size() && std::isdigit(static_cast<unsigned char>(path[end]))) {
            ++end;
        }
        profile_name = path.substr(0, end);
    }

    return !camera_host.empty();
}

bool SqliteSessionStore::insert_session_artifacts(const std::string& session_log_path,
                                                  const std::string& raw_metadata_path,
                                                  const std::string& parsed_summary_path,
                                                  std::string& error_message) {
    constexpr const char* sql = "INSERT OR REPLACE INTO session_artifacts (session_id, artifact_type, artifact_path) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        error_message = sqlite3_errmsg(db_);
        return false;
    }

    const std::vector<std::pair<std::string, std::string>> artifacts = {
        {"session_log", session_log_path},
        {"raw_metadata_log", raw_metadata_path},
        {"parsed_summary_log", parsed_summary_path},
    };

    for (const auto& artifact : artifacts) {
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        if (!bind_text(stmt, 1, session_id_) || !bind_text(stmt, 2, artifact.first) || !bind_text(stmt, 3, artifact.second)) {
            error_message = sqlite3_errmsg(db_);
            sqlite3_finalize(stmt);
            return false;
        }
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            error_message = sqlite3_errmsg(db_);
            sqlite3_finalize(stmt);
            return false;
        }
    }

    sqlite3_finalize(stmt);
    return true;
}

bool SqliteSessionStore::initialize(const std::string& output_root,
                                    const std::string& session_dir,
                                    const std::string& session_log_path,
                                    const std::string& raw_metadata_path,
                                    const std::string& parsed_summary_path,
                                    const AppConfig& config,
                                    const std::string& started_at,
                                    std::string& error_message) {
    close();

    try {
        const std::filesystem::path working_dir = std::filesystem::current_path();
        const std::filesystem::path configured_output_root(output_root);
        const std::filesystem::path resolved_output_root = configured_output_root.is_absolute()
            ? configured_output_root
            : (working_dir / configured_output_root);
        std::filesystem::create_directories(resolved_output_root);
        db_path_ = (resolved_output_root / "cvpp_review.db").string();
        session_id_ = std::filesystem::path(session_dir).filename().string();
    } catch (const std::exception& ex) {
        error_message = ex.what();
        return false;
    }

    if (sqlite3_open(db_path_.c_str(), &db_) != SQLITE_OK) {
        error_message = sqlite3_errmsg(db_);
        close();
        return false;
    }

    if (!prepare_schema(error_message)) {
        close();
        return false;
    }

    std::string camera_host;
    std::string profile_name;
    parse_rtsp_identity(config.rtsp_url, camera_host, profile_name);

    constexpr const char* session_sql = R"SQL(
INSERT OR REPLACE INTO sessions (
    session_id, started_at, ended_at, output_dir, camera_host, profile_name,
    metadata_enabled, raw_samples, parsed_payloads, malformed_payloads,
    event_only_payloads, detection_events, notes
) VALUES (?, ?, NULL, ?, ?, ?, ?, 0, 0, 0, 0, 0, NULL);
)SQL";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, session_sql, -1, &stmt, nullptr) != SQLITE_OK) {
        error_message = sqlite3_errmsg(db_);
        close();
        return false;
    }

    const bool bind_ok =
        bind_text(stmt, 1, session_id_) &&
        bind_text(stmt, 2, started_at) &&
        bind_text(stmt, 3, session_dir) &&
        bind_text(stmt, 4, camera_host) &&
        bind_text(stmt, 5, profile_name) &&
        sqlite3_bind_int(stmt, 6, config.enable_metadata ? 1 : 0) == SQLITE_OK;

    if (!bind_ok || sqlite3_step(stmt) != SQLITE_DONE) {
        error_message = sqlite3_errmsg(db_);
        sqlite3_finalize(stmt);
        close();
        return false;
    }
    sqlite3_finalize(stmt);

    if (!insert_session_artifacts(session_log_path, raw_metadata_path, parsed_summary_path, error_message)) {
        close();
        return false;
    }

    return true;
}

bool SqliteSessionStore::append_parsed_payload(const std::string& observed_at,
                                               const std::string& parse_status,
                                               const std::string& parse_message,
                                               const std::vector<DetectedObject>& objects,
                                               bool has_video_analytics,
                                               std::string& error_message) {
    if (!db_) {
        error_message = "sqlite store not initialized";
        return false;
    }

    if (!execute_sql("BEGIN IMMEDIATE TRANSACTION;", error_message)) {
        return false;
    }

    constexpr const char* payload_sql = R"SQL(
INSERT INTO parsed_payloads (
    session_id, observed_at, parse_status, parse_message, object_count,
    has_video_analytics, is_continuation_related
) VALUES (?, ?, ?, ?, ?, ?, ?);
)SQL";

    sqlite3_stmt* payload_stmt = nullptr;
    if (sqlite3_prepare_v2(db_, payload_sql, -1, &payload_stmt, nullptr) != SQLITE_OK) {
        error_message = sqlite3_errmsg(db_);
        execute_sql("ROLLBACK;", error_message);
        return false;
    }

    const bool continuation_related = parse_message.find("continuation") != std::string::npos;
    const bool payload_bind_ok =
        bind_text(payload_stmt, 1, session_id_) &&
        bind_text(payload_stmt, 2, observed_at) &&
        bind_text(payload_stmt, 3, parse_status) &&
        bind_text(payload_stmt, 4, parse_message) &&
        sqlite3_bind_int(payload_stmt, 5, static_cast<int>(objects.size())) == SQLITE_OK &&
        sqlite3_bind_int(payload_stmt, 6, has_video_analytics ? 1 : 0) == SQLITE_OK &&
        sqlite3_bind_int(payload_stmt, 7, continuation_related ? 1 : 0) == SQLITE_OK;

    if (!payload_bind_ok || sqlite3_step(payload_stmt) != SQLITE_DONE) {
        error_message = sqlite3_errmsg(db_);
        sqlite3_finalize(payload_stmt);
        execute_sql("ROLLBACK;", error_message);
        return false;
    }
    sqlite3_finalize(payload_stmt);

    const sqlite3_int64 payload_id = sqlite3_last_insert_rowid(db_);

    constexpr const char* object_sql = R"SQL(
INSERT INTO parsed_objects (
    payload_id, session_id, object_id, normalized_type, likelihood,
    left_coord, top_coord, right_coord, bottom_coord
) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);
)SQL";

    sqlite3_stmt* object_stmt = nullptr;
    if (sqlite3_prepare_v2(db_, object_sql, -1, &object_stmt, nullptr) != SQLITE_OK) {
        error_message = sqlite3_errmsg(db_);
        execute_sql("ROLLBACK;", error_message);
        return false;
    }

    for (const auto& object : objects) {
        sqlite3_reset(object_stmt);
        sqlite3_clear_bindings(object_stmt);
        const bool object_bind_ok =
            sqlite3_bind_int64(object_stmt, 1, payload_id) == SQLITE_OK &&
            bind_text(object_stmt, 2, session_id_) &&
            sqlite3_bind_int(object_stmt, 3, object.id) == SQLITE_OK &&
            bind_text(object_stmt, 4, object.type) &&
            sqlite3_bind_double(object_stmt, 5, object.likelihood) == SQLITE_OK &&
            sqlite3_bind_double(object_stmt, 6, object.left) == SQLITE_OK &&
            sqlite3_bind_double(object_stmt, 7, object.top) == SQLITE_OK &&
            sqlite3_bind_double(object_stmt, 8, object.right) == SQLITE_OK &&
            sqlite3_bind_double(object_stmt, 9, object.bottom) == SQLITE_OK;

        if (!object_bind_ok || sqlite3_step(object_stmt) != SQLITE_DONE) {
            error_message = sqlite3_errmsg(db_);
            sqlite3_finalize(object_stmt);
            execute_sql("ROLLBACK;", error_message);
            return false;
        }
    }
    sqlite3_finalize(object_stmt);

    if (!execute_sql("COMMIT;", error_message)) {
        execute_sql("ROLLBACK;", error_message);
        return false;
    }

    return true;
}

bool SqliteSessionStore::finalize_session(const std::string& ended_at,
                                          const SessionSummaryRow& summary,
                                          std::string& error_message) {
    if (!db_) {
        error_message = "sqlite store not initialized";
        return false;
    }

    if (!execute_sql("BEGIN IMMEDIATE TRANSACTION;", error_message)) {
        return false;
    }

    constexpr const char* session_update_sql = R"SQL(
UPDATE sessions
SET ended_at = ?,
    raw_samples = ?,
    parsed_payloads = ?,
    malformed_payloads = ?,
    event_only_payloads = ?,
    detection_events = ?
WHERE session_id = ?;
)SQL";

    sqlite3_stmt* update_stmt = nullptr;
    if (sqlite3_prepare_v2(db_, session_update_sql, -1, &update_stmt, nullptr) != SQLITE_OK) {
        error_message = sqlite3_errmsg(db_);
        execute_sql("ROLLBACK;", error_message);
        return false;
    }

    const bool update_bind_ok =
        bind_text(update_stmt, 1, ended_at) &&
        sqlite3_bind_int(update_stmt, 2, summary.raw_samples) == SQLITE_OK &&
        sqlite3_bind_int(update_stmt, 3, summary.parsed_payloads) == SQLITE_OK &&
        sqlite3_bind_int(update_stmt, 4, summary.malformed_payloads) == SQLITE_OK &&
        sqlite3_bind_int(update_stmt, 5, summary.event_only_payloads) == SQLITE_OK &&
        sqlite3_bind_int(update_stmt, 6, summary.detection_events) == SQLITE_OK &&
        bind_text(update_stmt, 7, session_id_);

    if (!update_bind_ok || sqlite3_step(update_stmt) != SQLITE_DONE) {
        error_message = sqlite3_errmsg(db_);
        sqlite3_finalize(update_stmt);
        execute_sql("ROLLBACK;", error_message);
        return false;
    }
    sqlite3_finalize(update_stmt);

    constexpr const char* delete_sql = "DELETE FROM session_type_metrics WHERE session_id = ?;";
    sqlite3_stmt* delete_stmt = nullptr;
    if (sqlite3_prepare_v2(db_, delete_sql, -1, &delete_stmt, nullptr) != SQLITE_OK) {
        error_message = sqlite3_errmsg(db_);
        execute_sql("ROLLBACK;", error_message);
        return false;
    }
    if (!bind_text(delete_stmt, 1, session_id_) || sqlite3_step(delete_stmt) != SQLITE_DONE) {
        error_message = sqlite3_errmsg(db_);
        sqlite3_finalize(delete_stmt);
        execute_sql("ROLLBACK;", error_message);
        return false;
    }
    sqlite3_finalize(delete_stmt);

    constexpr const char* metric_sql = R"SQL(
INSERT INTO session_type_metrics (
    session_id, normalized_type, detection_count, unique_object_count
) VALUES (?, ?, ?, ?);
)SQL";
    sqlite3_stmt* metric_stmt = nullptr;
    if (sqlite3_prepare_v2(db_, metric_sql, -1, &metric_stmt, nullptr) != SQLITE_OK) {
        error_message = sqlite3_errmsg(db_);
        execute_sql("ROLLBACK;", error_message);
        return false;
    }

    for (const auto& metric : summary.type_metrics) {
        sqlite3_reset(metric_stmt);
        sqlite3_clear_bindings(metric_stmt);
        const bool metric_bind_ok =
            bind_text(metric_stmt, 1, session_id_) &&
            bind_text(metric_stmt, 2, metric.normalized_type) &&
            sqlite3_bind_int(metric_stmt, 3, metric.detection_count) == SQLITE_OK &&
            sqlite3_bind_int(metric_stmt, 4, metric.unique_object_count) == SQLITE_OK;
        if (!metric_bind_ok || sqlite3_step(metric_stmt) != SQLITE_DONE) {
            error_message = sqlite3_errmsg(db_);
            sqlite3_finalize(metric_stmt);
            execute_sql("ROLLBACK;", error_message);
            return false;
        }
    }
    sqlite3_finalize(metric_stmt);

    if (!execute_sql("COMMIT;", error_message)) {
        execute_sql("ROLLBACK;", error_message);
        return false;
    }

    return true;
}
