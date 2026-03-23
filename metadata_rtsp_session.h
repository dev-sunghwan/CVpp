#pragma once

#include <chrono>
#include <string>

#include <gst/gst.h>
#include <gst/rtsp/rtsp.h>

#include "app_config.h"
#include "session_logger.h"
#include "shared_app_state.h"

class MetadataRtspSession {
public:
    MetadataRtspSession(const AppConfig& config, SessionLogger& logger, SharedAppState& state);
    ~MetadataRtspSession();

    bool start();
    void poll_bus_once(GstClockTime timeout = 10 * GST_MSECOND);
    void stop();
    bool enabled() const { return enabled_; }

private:
    static gboolean on_before_send(GstElement*, GstRTSPMessage* message, gpointer user_data);
    static gboolean on_select_stream(GstElement*, guint stream_index, GstCaps* caps, gpointer user_data);
    static void on_src_pad_added(GstElement*, GstPad* new_pad, gpointer user_data);
    static GstFlowReturn on_new_meta_sample(GstElement* sink, gpointer user_data);

    bool restart_after_startup_timeout();
    std::string caps_to_string(GstCaps* caps) const;
    void log_bus_message(GstMessage* msg);

    const AppConfig& config_;
    SessionLogger& logger_;
    SharedAppState& state_;
    bool enabled_ = false;
    GstElement* pipeline_ = nullptr;
    GstElement* meta_sink_ = nullptr;
    GstElement* video_sink_ = nullptr;
    int metadata_sample_count_ = 0;
    bool missing_metadata_warned_ = false;
    int startup_retry_count_ = 0;
    const int max_startup_retries_ = 2;
    std::chrono::steady_clock::time_point stream_started_at_{};
    std::string pending_xml_fragment_;
};
