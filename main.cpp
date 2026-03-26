#include <atomic>
#include <cctype>
#include <chrono>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include <opencv2/opencv.hpp>

#include "app_config.h"
#include "metadata_rtsp_session.h"
#include "metadata_types.h"
#include "session_logger.h"
#include "shared_app_state.h"
#include "video_rtsp_session.h"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <conio.h>
#endif

namespace {
std::atomic<bool> g_shutdown_requested = false;
constexpr int kOverlayHoldMs = 1500;
constexpr int kUiHeaderFont = cv::FONT_HERSHEY_DUPLEX;
constexpr int kUiBodyFont = cv::FONT_HERSHEY_DUPLEX;
constexpr int kUiMonoFont = cv::FONT_HERSHEY_PLAIN;
constexpr int kKeyTab = 9;
constexpr int kKeyEnter = 13;
constexpr int kKeyEsc = 27;
constexpr int kKeyBackspace = 8;
constexpr int kKeyUp = 2490368;
constexpr int kKeyDown = 2621440;

struct ConnectionFormData {
    std::string ip_address;
    std::string username;
    std::string password;
    std::string profile = "2";
};

#ifdef _WIN32
BOOL WINAPI console_ctrl_handler(DWORD ctrl_type) {
    switch (ctrl_type) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            g_shutdown_requested = true;
            return TRUE;
        default:
            return FALSE;
    }
}
#endif

void draw_overlay(cv::Mat& frame, const std::vector<DetectedObject>& objects) {
    const int fw = frame.cols;
    const int fh = frame.rows;

    for (const auto& obj : objects) {
        int x1 = static_cast<int>(obj.left);
        int y1 = static_cast<int>(obj.top);
        int x2 = static_cast<int>(obj.right);
        int y2 = static_cast<int>(obj.bottom);

        auto clampI = [](int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); };
        x1 = clampI(x1, 0, fw - 1);
        y1 = clampI(y1, 0, fh - 1);
        x2 = clampI(x2, 0, fw - 1);
        y2 = clampI(y2, 0, fh - 1);

        cv::rectangle(frame, cv::Point(x1, y1), cv::Point(x2, y2), cv::Scalar(0, 255, 0), 2);

        const int confidence = static_cast<int>(obj.likelihood * 100);
        const std::string label = obj.type == "Unknown"
            ? ("ID " + std::to_string(obj.id) + " " + std::to_string(confidence) + "%")
            : (obj.type + " " + std::to_string(confidence) + "%");
        int baseline = 0;
        const cv::Size text_size = cv::getTextSize(label, kUiBodyFont, 0.5, 1, &baseline);
        int label_y = (y1 - 5 > text_size.height + 5) ? (y1 - 5) : (text_size.height + 5);
        int label_x = x1;

        const int max_label_x = (fw - text_size.width - 1 > 0) ? (fw - text_size.width - 1) : 0;
        const int min_label_y = text_size.height + baseline + 4;
        const int max_label_y = (fh - 1 > min_label_y) ? (fh - 1) : min_label_y;
        label_x = clampI(label_x, 0, max_label_x);
        label_y = clampI(label_y, min_label_y, max_label_y);

        cv::rectangle(frame,
                      cv::Point(label_x, label_y - text_size.height - baseline - 4),
                      cv::Point(label_x + text_size.width, label_y + baseline),
                      cv::Scalar(0, 200, 0), cv::FILLED);
        cv::putText(frame, label, cv::Point(label_x, label_y - 2), kUiBodyFont, 0.5, cv::Scalar(0, 0, 0), 1);
    }
}

int draw_text_block(cv::Mat& canvas,
                    const std::string& text,
                    int x,
                    int y,
                    int max_width,
                    double font_scale,
                    const cv::Scalar& color,
                    int thickness = 1,
                    int font_face = kUiBodyFont) {
    std::istringstream words(text);
    std::string word;
    std::string line;
    int baseline = 0;
    const int line_gap = 6;
    int current_y = y;

    while (words >> word) {
        const std::string candidate = line.empty() ? word : (line + " " + word);
        const cv::Size text_size = cv::getTextSize(candidate, font_face, font_scale, thickness, &baseline);
        if (!line.empty() && text_size.width > max_width) {
            cv::putText(canvas, line, cv::Point(x, current_y), font_face, font_scale, color, thickness);
            current_y += text_size.height + line_gap;
            line = word;
        } else {
            line = candidate;
        }
    }

    if (!line.empty()) {
        const cv::Size text_size = cv::getTextSize(line, font_face, font_scale, thickness, &baseline);
        cv::putText(canvas, line, cv::Point(x, current_y), font_face, font_scale, color, thickness);
        current_y += text_size.height + line_gap;
    }

    return current_y;
}

int draw_panel_section(cv::Mat& canvas, int x, int y, int width, const std::string& title) {
    cv::rectangle(canvas, cv::Point(x, y), cv::Point(x + width, y + 36), cv::Scalar(38, 55, 73), cv::FILLED);
    cv::putText(canvas, title, cv::Point(x + 12, y + 24), kUiHeaderFont, 0.62, cv::Scalar(235, 240, 245), 1);
    return y + 52;
}

std::map<std::string, int> to_sorted_map(const std::unordered_map<std::string, int>& source) {
    return std::map<std::string, int>(source.begin(), source.end());
}

std::map<std::string, int> to_sorted_unique_count_map(const std::unordered_map<std::string, std::unordered_set<int>>& source) {
    std::map<std::string, int> result;
    for (const auto& entry : source) {
        result[entry.first] = static_cast<int>(entry.second.size());
    }
    return result;
}

cv::Mat compose_verification_ui(const cv::Mat& video_frame,
                                const std::string& status_text,
                                const std::string& metadata_state_text,
                                const std::vector<std::string>& evidence_lines,
                                const std::map<std::string, int>& detections_by_type,
                                const std::map<std::string, int>& unique_ids_by_type,
                                const std::vector<std::string>& recent_summaries) {
    constexpr int kPanelWidth = 460;
    constexpr int kOuterMargin = 14;
    constexpr int kPanelGap = 14;

    cv::Mat canvas(video_frame.rows, video_frame.cols + kPanelWidth + kPanelGap + (kOuterMargin * 2),
                   CV_8UC3, cv::Scalar(18, 22, 27));
    video_frame.copyTo(canvas(cv::Rect(kOuterMargin, 0, video_frame.cols, video_frame.rows)));

    const int panel_x = kOuterMargin + video_frame.cols + kPanelGap;
    const int panel_width = kPanelWidth - kOuterMargin;
    int cursor_y = kOuterMargin;

    cv::rectangle(canvas,
                  cv::Point(panel_x - 10, kOuterMargin),
                  cv::Point(panel_x + panel_width, video_frame.rows - kOuterMargin),
                  cv::Scalar(24, 29, 36), cv::FILLED);

    cursor_y = draw_panel_section(canvas, panel_x, cursor_y, panel_width - 10, "Evidence");
    cursor_y = draw_text_block(canvas, status_text, panel_x + 12, cursor_y, panel_width - 34, 0.52, cv::Scalar(0, 255, 255));
    cursor_y += 6;
    cursor_y = draw_text_block(canvas, metadata_state_text, panel_x + 12, cursor_y, panel_width - 34, 0.48, cv::Scalar(230, 230, 230));
    cursor_y += 8;
    for (const auto& line : evidence_lines) {
        cursor_y = draw_text_block(canvas, line, panel_x + 12, cursor_y, panel_width - 34, 0.46, cv::Scalar(210, 220, 230));
    }

    cursor_y += 10;
    cursor_y = draw_panel_section(canvas, panel_x, cursor_y, panel_width - 10, "Session Metrics");
    if (detections_by_type.empty()) {
        cursor_y = draw_text_block(canvas, "No detection metrics yet.", panel_x + 12, cursor_y, panel_width - 34, 0.48, cv::Scalar(180, 190, 200));
    } else {
        cursor_y = draw_text_block(canvas, "Detections by type", panel_x + 12, cursor_y, panel_width - 34, 0.48, cv::Scalar(255, 255, 255));
        for (const auto& entry : detections_by_type) {
            cursor_y = draw_text_block(canvas, entry.first + ": " + std::to_string(entry.second),
                                       panel_x + 22, cursor_y, panel_width - 44, 0.44, cv::Scalar(205, 215, 225));
        }
        cursor_y += 6;
        cursor_y = draw_text_block(canvas, "Unique object IDs", panel_x + 12, cursor_y, panel_width - 34, 0.48, cv::Scalar(255, 255, 255));
        for (const auto& entry : unique_ids_by_type) {
            cursor_y = draw_text_block(canvas, entry.first + ": " + std::to_string(entry.second),
                                       panel_x + 22, cursor_y, panel_width - 44, 0.44, cv::Scalar(205, 215, 225));
        }
    }

    cursor_y += 10;
    if (cursor_y < video_frame.rows - 120) {
        cursor_y = draw_panel_section(canvas, panel_x, cursor_y, panel_width - 10, "Recent Metadata");
        if (recent_summaries.empty()) {
            draw_text_block(canvas, "No parsed metadata yet.", panel_x + 12, cursor_y, panel_width - 34, 0.48, cv::Scalar(180, 190, 200));
        } else {
            for (const auto& line : recent_summaries) {
                cursor_y = draw_text_block(canvas, line, panel_x + 12, cursor_y, panel_width - 34, 0.41, cv::Scalar(205, 215, 225));
                cursor_y += 2;
                if (cursor_y > video_frame.rows - 28) {
                    break;
                }
            }
        }
    }

    return canvas;
}

cv::Mat fit_for_display(const cv::Mat& image) {
    constexpr int kMaxDisplayWidth = 1600;
    constexpr int kMaxDisplayHeight = 900;

    if (image.cols <= kMaxDisplayWidth && image.rows <= kMaxDisplayHeight) {
        return image;
    }

    const double scale_x = static_cast<double>(kMaxDisplayWidth) / static_cast<double>(image.cols);
    const double scale_y = static_cast<double>(kMaxDisplayHeight) / static_cast<double>(image.rows);
    const double scale = std::min(scale_x, scale_y);

    cv::Mat resized;
    cv::resize(image, resized, cv::Size(), scale, scale, cv::INTER_AREA);
    return resized;
}

bool parse_rtsp_connection_defaults(const std::string& rtsp_url, ConnectionFormData& form) {
    const std::string prefix = "rtsp://";
    if (rtsp_url.rfind(prefix, 0) != 0) {
        return false;
    }

    const std::string remainder = rtsp_url.substr(prefix.size());
    const size_t at_pos = remainder.find('@');
    const size_t slash_pos = remainder.find('/');
    if (at_pos == std::string::npos || slash_pos == std::string::npos || at_pos >= slash_pos) {
        return false;
    }

    const std::string credentials = remainder.substr(0, at_pos);
    const std::string host = remainder.substr(at_pos + 1, slash_pos - at_pos - 1);
    const std::string path = remainder.substr(slash_pos + 1);

    const size_t colon_pos = credentials.find(':');
    if (colon_pos != std::string::npos) {
        form.username = credentials.substr(0, colon_pos);
        form.password = credentials.substr(colon_pos + 1);
    } else {
        form.username = credentials;
    }
    form.ip_address = host;

    const std::string profile_prefix = "profile";
    if (path.rfind(profile_prefix, 0) == 0) {
        size_t profile_end = profile_prefix.size();
        while (profile_end < path.size() && std::isdigit(static_cast<unsigned char>(path[profile_end]))) {
            ++profile_end;
        }
        form.profile = path.substr(profile_prefix.size(), profile_end - profile_prefix.size());
    }

    return !form.ip_address.empty();
}

std::string masked_password(const std::string& password) {
    return password.empty() ? std::string() : std::string(password.size(), '*');
}

std::string build_rtsp_url(const ConnectionFormData& form) {
    return "rtsp://" + form.username + ":" + form.password + "@" + form.ip_address +
           "/profile" + form.profile + "/media.smp";
}

cv::Mat render_connection_setup(const ConnectionFormData& form, int active_field) {
    cv::Mat canvas(430, 860, CV_8UC3, cv::Scalar(18, 22, 27));
    cv::rectangle(canvas, cv::Point(24, 24), cv::Point(836, 406), cv::Scalar(24, 29, 36), cv::FILLED);

    cv::putText(canvas, "CV++ Camera Connection", cv::Point(44, 64), kUiHeaderFont, 0.9, cv::Scalar(240, 244, 248), 1);
    cv::putText(canvas, "Enter connection info for this run only.", cv::Point(44, 96), kUiBodyFont, 0.58, cv::Scalar(190, 200, 210), 1);

    const std::vector<std::pair<std::string, std::string>> fields = {
        {"IP Address", form.ip_address},
        {"Username", form.username},
        {"Password", masked_password(form.password)},
        {"Profile", form.profile}
    };

    int y = 138;
    for (size_t i = 0; i < fields.size(); ++i) {
        const bool active = static_cast<int>(i) == active_field;
        cv::putText(canvas, fields[i].first, cv::Point(48, y), kUiBodyFont, 0.58,
                    active ? cv::Scalar(255, 255, 255) : cv::Scalar(185, 195, 205), 1);
        cv::rectangle(canvas, cv::Point(210, y - 24), cv::Point(790, y + 12),
                      active ? cv::Scalar(70, 118, 170) : cv::Scalar(46, 54, 66), 2);
        cv::putText(canvas, fields[i].second.empty() ? "<empty>" : fields[i].second,
                    cv::Point(224, y), kUiMonoFont, 1.25,
                    fields[i].second.empty() ? cv::Scalar(120, 130, 140) : cv::Scalar(238, 241, 244), 1);
        y += 64;
    }

    cv::putText(canvas, "TAB / Up / Down: move field", cv::Point(48, 340), kUiBodyFont, 0.5, cv::Scalar(190, 200, 210), 1);
    cv::putText(canvas, "Type to edit, Backspace to delete", cv::Point(48, 366), kUiBodyFont, 0.5, cv::Scalar(190, 200, 210), 1);
    cv::putText(canvas, "Enter: connect using rtsp://<user>:<password>@<ip>/profile<profile>/media.smp",
                cv::Point(48, 392), kUiBodyFont, 0.48, cv::Scalar(190, 200, 210), 1);

    return canvas;
}

bool run_connection_setup(AppConfig& config) {
    if (const char* skip_setup = std::getenv("CVPP_SKIP_CONNECTION_SETUP")) {
        if (std::string(skip_setup) == "1") {
            return true;
        }
    }

    ConnectionFormData form;
    parse_rtsp_connection_defaults(config.rtsp_url, form);
    if (form.profile.empty()) {
        form.profile = "2";
    }
    int active_field = 0;
    cv::namedWindow("CV++ Connection Setup", cv::WINDOW_NORMAL);
    cv::resizeWindow("CV++ Connection Setup", 1100, 620);

    while (true) {
        cv::imshow("CV++ Connection Setup", render_connection_setup(form, active_field));
        const int key = cv::waitKey(30);
        if (key < 0) {
            continue;
        }

        if (key == kKeyEsc) {
            cv::destroyWindow("CV++ Connection Setup");
            return false;
        }
        if (key == kKeyTab || key == kKeyDown) {
            active_field = (active_field + 1) % 4;
            continue;
        }
        if (key == kKeyUp) {
            active_field = (active_field + 3) % 4;
            continue;
        }
        if (key == kKeyEnter) {
            if (!form.ip_address.empty() && !form.username.empty() && !form.password.empty() && !form.profile.empty()) {
                config.rtsp_url = build_rtsp_url(form);
                cv::destroyWindow("CV++ Connection Setup");
                return true;
            }
            continue;
        }

        std::string* target = nullptr;
        switch (active_field) {
            case 0: target = &form.ip_address; break;
            case 1: target = &form.username; break;
            case 2: target = &form.password; break;
            case 3: target = &form.profile; break;
            default: break;
        }
        if (!target) {
            continue;
        }

        if (key == kKeyBackspace) {
            if (!target->empty()) {
                target->pop_back();
            }
            continue;
        }

        if (key >= 32 && key <= 126) {
            const char ch = static_cast<char>(key);
            if (active_field == 3) {
                if (std::isdigit(static_cast<unsigned char>(ch))) {
                    target->push_back(ch);
                }
            } else {
                target->push_back(ch);
            }
        }
    }
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

void log_session_metrics(SessionLogger& logger, SharedAppState& state) {
    std::map<std::string, int> detections_by_type;
    std::map<std::string, int> unique_ids_by_type;
    int total_raw = 0;
    int total_parsed = 0;
    int total_malformed = 0;
    int total_event_only = 0;
    int total_detections = 0;

    {
        std::lock_guard<std::mutex> lock(state.meta_mutex);
        total_raw = state.total_raw_metadata_samples;
        total_parsed = state.total_parsed_payloads;
        total_malformed = state.total_malformed_payloads;
        total_event_only = state.total_event_only_payloads;
        total_detections = state.total_detection_events;

        for (const auto& entry : state.detections_by_type) {
            detections_by_type[entry.first] = entry.second;
        }
        for (const auto& entry : state.unique_ids_by_type) {
            unique_ids_by_type[entry.first] = static_cast<int>(entry.second.size());
        }
    }

    std::ostringstream headline;
    headline << "Session metrics: raw_samples=" << total_raw
             << " parsed_payloads=" << total_parsed
             << " malformed=" << total_malformed
             << " event_only=" << total_event_only
             << " detection_events=" << total_detections;
    logger.log_event(headline.str());
    logger.log_event(std::string("Session metrics: detections_by_type=") + format_type_counts(detections_by_type));
    logger.log_event(std::string("Session metrics: unique_ids_by_type=") + format_type_counts(unique_ids_by_type));
}
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    std::cout << "==================================================" << std::endl;
    std::cout << "[MAIN] GStreamer Video & Metadata Analytics" << std::endl;
    std::cout << "==================================================" << std::endl;

    AppConfig config;
    std::string error_message;
    if (!load_config("config.toml", config, error_message)) {
        std::cerr << "[ERROR] " << error_message << std::endl;
        return 1;
    }

    if (!run_connection_setup(config)) {
        std::cout << "[INFO] Connection setup cancelled." << std::endl;
        return 0;
    }

    SessionLogger logger;
    if (!logger.initialize(config.output_root, error_message)) {
        std::cerr << "[ERROR] Failed to initialize logging: " << error_message << std::endl;
        return 1;
    }

    SharedAppState shared_state;

    std::cout << "[INFO] Session output directory: " << logger.session_dir() << std::endl;
    logger.log_event(std::string("Runtime direction: split sessions | metadata ") + (config.enable_metadata ? "enabled" : "disabled"));
    logger.log_event("Config loaded from config.toml");
    logger.log_event(std::string("Fixture candidate capture: ") + (config.capture_fixture_candidates ? "enabled" : "disabled"));
    logger.log_event(std::string("Runtime RTSP target: ") + config.rtsp_url);

#ifdef _WIN32
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
#endif

    gst_init(&argc, &argv);
    std::cout << "[INFO] GStreamer core engine initialized." << std::endl;

    VideoRtspSession video_session(config, logger, shared_state);
    MetadataRtspSession metadata_session(config, logger, shared_state);

    if (!video_session.start()) {
        std::cerr << "[ERROR] Failed to start VideoRtspSession." << std::endl;
        return 1;
    }
    bool metadata_started = false;
    constexpr auto kMetadataStartDelay = std::chrono::milliseconds(1500);
    if (config.enable_metadata) {
        std::cout << "[INFO] Waiting for first video frame before starting metadata session..." << std::endl;
        logger.log_event("Main: waiting for first video frame before starting metadata session");

        const auto startup_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
        std::chrono::steady_clock::time_point video_ready_since{};
        while (std::chrono::steady_clock::now() < startup_deadline) {
            video_session.poll_bus_once(50 * GST_MSECOND);

            bool video_ready = false;
            {
                std::lock_guard<std::mutex> lock(shared_state.frame_mutex);
                video_ready = !shared_state.current_frame.empty();
            }

            if (video_ready) {
                if (video_ready_since == std::chrono::steady_clock::time_point{}) {
                    video_ready_since = std::chrono::steady_clock::now();
                    logger.log_event("Main: first video frame observed; delaying metadata startup briefly for a stable video baseline");
                }
                if (std::chrono::steady_clock::now() - video_ready_since >= kMetadataStartDelay) {
                    break;
                }
            } else {
                video_ready_since = std::chrono::steady_clock::time_point{};
            }

#ifdef _WIN32
            if (g_shutdown_requested.load()) {
                break;
            }
#endif
        }

        if (!metadata_session.start()) {
            std::cerr << "[ERROR] Failed to start MetadataRtspSession." << std::endl;
            return 1;
        }
        metadata_started = true;
    } else if (!metadata_session.start()) {
        std::cerr << "[ERROR] Failed to start MetadataRtspSession." << std::endl;
        return 1;
    }

    std::cout << "[INFO] Streaming. Press ESC to exit." << std::endl;
    cv::namedWindow("CV++ Verification View (ESC to quit)", cv::WINDOW_NORMAL);

    while (true) {
        video_session.poll_bus_once();
        if (metadata_started) {
            metadata_session.poll_bus_once();
        }

        cv::Mat display_frame;
        {
            std::lock_guard<std::mutex> lock(shared_state.frame_mutex);
            if (shared_state.new_frame_available) {
                display_frame = shared_state.current_frame.clone();
                shared_state.new_frame_available = false;
            }
        }

        if (!display_frame.empty()) {
            std::vector<DetectedObject> overlay_objects;
            bool metadata_is_fresh = false;
            std::string status_text;
            int total_raw_metadata_samples = 0;
            int last_parsed_object_count = 0;
            int total_parsed_payloads = 0;
            int total_malformed_payloads = 0;
            int total_event_only_payloads = 0;
            int total_detection_events = 0;
            std::chrono::steady_clock::time_point last_raw_metadata_seen{};
            std::map<std::string, int> detections_by_type;
            std::map<std::string, int> unique_ids_by_type;
            std::vector<std::string> recent_summaries;

            {
                std::lock_guard<std::mutex> lock(shared_state.meta_mutex);
                const auto fresh_now = std::chrono::steady_clock::now();
                metadata_is_fresh =
                    shared_state.has_metadata_update &&
                    (std::chrono::duration_cast<std::chrono::milliseconds>(fresh_now - shared_state.last_metadata_update).count() <= kOverlayHoldMs);
                status_text = shared_state.last_parse_status_text;
                total_raw_metadata_samples = shared_state.total_raw_metadata_samples;
                last_parsed_object_count = shared_state.last_parsed_object_count;
                total_parsed_payloads = shared_state.total_parsed_payloads;
                total_malformed_payloads = shared_state.total_malformed_payloads;
                total_event_only_payloads = shared_state.total_event_only_payloads;
                total_detection_events = shared_state.total_detection_events;
                last_raw_metadata_seen = shared_state.last_raw_metadata_seen;
                detections_by_type = to_sorted_map(shared_state.detections_by_type);
                unique_ids_by_type = to_sorted_unique_count_map(shared_state.unique_ids_by_type);
                recent_summaries = shared_state.recent_parsed_summaries;

                if (metadata_is_fresh) {
                    overlay_objects = shared_state.current_objects;
                }
            }

            if (!overlay_objects.empty()) {
                draw_overlay(display_frame, overlay_objects);
            }

            const auto metadata_age_ms = total_raw_metadata_samples > 0
                ? std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now() - last_raw_metadata_seen).count()
                : -1LL;
            const std::string metadata_state_text =
                std::string("raw=") + (total_raw_metadata_samples > 0 ? "seen" : "not-seen") +
                " | parsed=" + std::to_string(last_parsed_object_count) +
                " | overlay=" + std::to_string(static_cast<int>(overlay_objects.size())) +
                " | age=" + (metadata_age_ms >= 0 ? std::to_string(metadata_age_ms) + "ms" : std::string("n/a")) +
                " | fresh=" + (metadata_is_fresh ? "yes" : "no");

            std::vector<std::string> evidence_lines;
            evidence_lines.push_back("Raw payloads: " + std::to_string(total_raw_metadata_samples));
            evidence_lines.push_back("Parsed payloads: " + std::to_string(total_parsed_payloads));
            evidence_lines.push_back("Malformed payloads: " + std::to_string(total_malformed_payloads));
            evidence_lines.push_back("Event-only payloads: " + std::to_string(total_event_only_payloads));
            evidence_lines.push_back("Detection events: " + std::to_string(total_detection_events));

            cv::Mat verification_ui = compose_verification_ui(
                display_frame,
                status_text,
                metadata_state_text,
                evidence_lines,
                detections_by_type,
                unique_ids_by_type,
                recent_summaries);

            cv::Mat fitted_ui = fit_for_display(verification_ui);
            cv::imshow("CV++ Verification View (ESC to quit)", fitted_ui);
        }

        const int window_key = cv::waitKey(30);
        if (window_key == kKeyEsc) {
            std::cout << "\n[MAIN] ESC pressed in display window. Shutting down." << std::endl;
            logger.log_event("ESC pressed in display window. Shutting down.");
            break;
        }

#ifdef _WIN32
        if (_kbhit()) {
            const int console_key = _getch();
            if (console_key == kKeyEsc || console_key == 'q' || console_key == 'Q') {
                std::cout << "\n[MAIN] Console quit key pressed. Shutting down." << std::endl;
                logger.log_event("Console quit key pressed. Shutting down.");
                break;
            }
        }

        if (g_shutdown_requested.load()) {
            std::cout << "\n[MAIN] Console control event received. Shutting down." << std::endl;
            logger.log_event("Console control event received. Shutting down.");
            break;
        }
#endif
    }

    if (metadata_started) {
        metadata_session.stop();
    }
    video_session.stop();
    cv::destroyAllWindows();
    log_session_metrics(logger, shared_state);
    logger.log_event("Session stopped");
    std::cout << "[INFO] Done." << std::endl;
    return 0;
}




