#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>
#include <iostream>

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
    int fw = frame.cols;
    int fh = frame.rows;

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

        int confidence = static_cast<int>(obj.likelihood * 100);
        std::string label = obj.type == "Unknown"
            ? ("ID " + std::to_string(obj.id) + " " + std::to_string(confidence) + "%")
            : (obj.type + " " + std::to_string(confidence) + "%");
        int baseline = 0;
        cv::Size text_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);
        int label_y = (y1 - 5 > text_size.height + 5) ? (y1 - 5) : (text_size.height + 5);
        int label_x = x1;

        int max_label_x = (fw - text_size.width - 1 > 0) ? (fw - text_size.width - 1) : 0;
        int min_label_y = text_size.height + baseline + 4;
        int max_label_y = (fh - 1 > min_label_y) ? (fh - 1) : min_label_y;
        label_x = clampI(label_x, 0, max_label_x);
        label_y = clampI(label_y, min_label_y, max_label_y);

        cv::rectangle(frame,
                      cv::Point(label_x, label_y - text_size.height - baseline - 4),
                      cv::Point(label_x + text_size.width, label_y + baseline),
                      cv::Scalar(0, 200, 0), cv::FILLED);
        cv::putText(frame, label, cv::Point(label_x, label_y - 2),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0), 1);
    }
}

void draw_status_banner(cv::Mat& frame, const std::string& status_text) {
    cv::rectangle(frame, cv::Point(10, 10), cv::Point(frame.cols - 10, 40), cv::Scalar(20, 20, 20), cv::FILLED);
    cv::putText(frame, status_text, cv::Point(20, 33), cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(0, 255, 255), 1);
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

    if (!metadata_session.start()) {
        std::cerr << "[ERROR] Failed to start MetadataRtspSession." << std::endl;
        return 1;
    }

    std::cout << "[INFO] Streaming. Press ESC to exit." << std::endl;

    while (true) {
        video_session.poll_bus_once();
        metadata_session.poll_bus_once();

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
            {
                std::lock_guard<std::mutex> lock(shared_state.meta_mutex);
                const auto fresh_now = std::chrono::steady_clock::now();
                const bool metadata_is_fresh =
                    shared_state.has_metadata_update &&
                    (std::chrono::duration_cast<std::chrono::milliseconds>(fresh_now - shared_state.last_metadata_update).count() <= 500);

                if (metadata_is_fresh) {
                    overlay_objects = shared_state.current_objects;
                }
            }
            if (!overlay_objects.empty()) {
                draw_overlay(display_frame, overlay_objects);
            }
            draw_status_banner(display_frame, shared_state.last_parse_status_text);
            cv::imshow("GStreamer Analytics (ESC to quit)", display_frame);
        }

        const int window_key = cv::waitKey(30);
        if (window_key == 27) {
            std::cout << "\n[MAIN] ESC pressed in display window. Shutting down." << std::endl;
            logger.log_event("ESC pressed in display window. Shutting down.");
            break;
        }

#ifdef _WIN32
        if (_kbhit()) {
            const int console_key = _getch();
            if (console_key == 27 || console_key == 'q' || console_key == 'Q') {
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

    metadata_session.stop();
    video_session.stop();
    cv::destroyAllWindows();
    logger.log_event("Session stopped");
    std::cout << "[INFO] Done." << std::endl;
    return 0;
}
