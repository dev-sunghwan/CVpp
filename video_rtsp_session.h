#pragma once

#include <chrono>

#include <gst/gst.h>
#include <gst/rtsp/rtsp.h>

#include "app_config.h"
#include "session_logger.h"
#include "shared_app_state.h"

class VideoRtspSession {
public:
    VideoRtspSession(const AppConfig& config, SessionLogger& logger, SharedAppState& state);
    ~VideoRtspSession();

    bool start();
    void poll_bus_once(GstClockTime timeout = 10 * GST_MSECOND);
    void stop();

private:
    static gboolean on_before_send(GstElement*, GstRTSPMessage* message, gpointer user_data);
    static void on_src_pad_added(GstElement*, GstPad* new_pad, gpointer user_data);
    static void on_decodebin_pad_added(GstElement*, GstPad* new_pad, gpointer user_data);
    static GstFlowReturn on_new_video_sample(GstElement* sink, gpointer user_data);

    bool restart_after_startup_timeout();
    std::string caps_to_string(GstCaps* caps) const;
    void log_bus_message(GstMessage* msg);

    const AppConfig& config_;
    SessionLogger& logger_;
    SharedAppState& state_;
    GstElement* pipeline_ = nullptr;
    GstElement* video_queue_ = nullptr;
    GstElement* decodebin_ = nullptr;
    GstElement* vconv_ = nullptr;
    GstElement* video_sink_ = nullptr;
    int video_sample_count_ = 0;
    bool missing_video_warned_ = false;
    int startup_retry_count_ = 0;
    const int max_startup_retries_ = 2;
    std::chrono::steady_clock::time_point stream_started_at_{};
};
