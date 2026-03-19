#include "video_rtsp_session.h"

#include <iostream>
#include <sstream>

#include <gst/app/gstappsink.h>
#include <gst/rtsp/rtsp.h>

namespace {
const char* rtsp_method_label(GstRTSPMethod method) {
    switch (method) {
        case GST_RTSP_OPTIONS: return "OPTIONS";
        case GST_RTSP_DESCRIBE: return "DESCRIBE";
        case GST_RTSP_PAUSE: return "PAUSE";
        case GST_RTSP_PLAY: return "PLAY";
        case GST_RTSP_SETUP: return "SETUP";
        case GST_RTSP_TEARDOWN: return "TEARDOWN";
        default: return "UNKNOWN";
    }
}
}

VideoRtspSession::VideoRtspSession(const AppConfig& config, SessionLogger& logger, SharedAppState& state)
    : config_(config), logger_(logger), state_(state) {}

VideoRtspSession::~VideoRtspSession() {
    stop();
}

std::string VideoRtspSession::caps_to_string(GstCaps* caps) const {
    if (!caps) {
        return "<no-caps>";
    }

    gchar* text = gst_caps_to_string(caps);
    std::string result = text ? text : "<caps-to-string-failed>";
    if (text) {
        g_free(text);
    }
    return result;
}

bool VideoRtspSession::start() {
    pipeline_ = gst_pipeline_new("video_pipeline");
    GstElement* rtspsrc = gst_element_factory_make("rtspsrc", "video_src");
    video_queue_ = gst_element_factory_make("queue", "video_queue");
    GstElement* h264_depay = gst_element_factory_make("rtph264depay", "video_h264_depay");
    GstElement* h264_parse = gst_element_factory_make("h264parse", "video_h264_parse");
    decodebin_ = gst_element_factory_make("decodebin", "video_decodebin");
    vconv_ = gst_element_factory_make("videoconvert", "video_vconv");
    video_sink_ = gst_element_factory_make("appsink", "video_sink");

    if (!pipeline_ || !rtspsrc || !video_queue_ || !h264_depay || !h264_parse || !decodebin_ || !vconv_ || !video_sink_) {
        logger_.log_event("VideoSession: failed to create GStreamer elements");
        return false;
    }

    g_object_set(G_OBJECT(rtspsrc),
                 "location", config_.rtsp_url.c_str(),
                 "latency", config_.latency,
                 "protocols", GST_RTSP_LOWER_TRANS_TCP,
                 NULL);
    g_signal_connect(rtspsrc, "before-send", G_CALLBACK(VideoRtspSession::on_before_send), this);
    g_signal_connect(rtspsrc, "select-stream", G_CALLBACK(VideoRtspSession::on_select_stream), this);

    GstCaps* caps_v = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "BGR", NULL);
    g_object_set(G_OBJECT(video_sink_), "caps", caps_v, "emit-signals", TRUE, "sync", FALSE, "async", FALSE,
                 "max-buffers", 2, "drop", TRUE, NULL);
    gst_caps_unref(caps_v);
    g_object_set(G_OBJECT(video_queue_), "leaky", 2, "max-size-buffers", 8, NULL);

    gst_bin_add_many(GST_BIN(pipeline_), rtspsrc, video_queue_, h264_depay, h264_parse, decodebin_, vconv_, video_sink_, NULL);

    if (!gst_element_link_many(video_queue_, h264_depay, h264_parse, decodebin_, NULL)) {
        logger_.log_event("VideoSession: failed to link explicit H264 path to decodebin");
        return false;
    }

    if (!gst_element_link(vconv_, video_sink_)) {
        logger_.log_event("VideoSession: failed to link videoconvert to appsink");
        return false;
    }

    g_signal_connect(rtspsrc, "pad-added", G_CALLBACK(VideoRtspSession::on_src_pad_added), this);
    g_signal_connect(decodebin_, "pad-added", G_CALLBACK(VideoRtspSession::on_decodebin_pad_added), this);
    g_signal_connect(video_sink_, "new-sample", G_CALLBACK(VideoRtspSession::on_new_video_sample), this);

    logger_.log_event("VideoSession: starting pipeline");
    GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        logger_.log_event("VideoSession: failed to change pipeline state to PLAYING");
        return false;
    }

    video_sample_count_ = 0;
    missing_video_warned_ = false;
    startup_retry_count_ = 0;
    stream_started_at_ = std::chrono::steady_clock::now();
    return true;
}

void VideoRtspSession::stop() {
    if (!pipeline_) {
        return;
    }

    logger_.log_event("VideoSession: requesting pipeline shutdown (GST_STATE_NULL)");
    GstStateChangeReturn shutdown_ret = gst_element_set_state(pipeline_, GST_STATE_NULL);
    logger_.log_event(std::string("VideoSession: shutdown state change return: ") + gst_element_state_change_return_get_name(shutdown_ret));

    GstState current_state = GST_STATE_VOID_PENDING;
    GstState pending_state = GST_STATE_VOID_PENDING;
    GstStateChangeReturn wait_ret = gst_element_get_state(pipeline_, &current_state, &pending_state, 2 * GST_SECOND);
    std::ostringstream shutdown_text;
    shutdown_text << "VideoSession: shutdown wait result: " << gst_element_state_change_return_get_name(wait_ret)
                  << " | current=" << gst_element_state_get_name(current_state)
                  << " | pending=" << gst_element_state_get_name(pending_state);
    logger_.log_event(shutdown_text.str());

    gst_object_unref(pipeline_);
    pipeline_ = nullptr;
    video_queue_ = nullptr;
    decodebin_ = nullptr;
    vconv_ = nullptr;
    video_sink_ = nullptr;
}

void VideoRtspSession::poll_bus_once(GstClockTime timeout) {
    if (!pipeline_) {
        return;
    }

    GstBus* bus = gst_element_get_bus(pipeline_);
    GstMessage* msg = gst_bus_timed_pop(bus, timeout);
    if (msg) {
        log_bus_message(msg);
        gst_message_unref(msg);
    }
    gst_object_unref(bus);

    const auto now = std::chrono::steady_clock::now();
    if (!missing_video_warned_ && video_sample_count_ == 0 &&
        std::chrono::duration_cast<std::chrono::seconds>(now - stream_started_at_).count() >= 10) {
        std::cout << "[WARN] No video samples received within 10 seconds." << std::endl;
        logger_.log_event("VideoSession: no video samples received within 10 seconds");
        missing_video_warned_ = true;
        restart_after_startup_timeout();
    }
}

bool VideoRtspSession::restart_after_startup_timeout() {
    if (startup_retry_count_ >= max_startup_retries_) {
        logger_.log_event("VideoSession: startup watchdog retries exhausted; keeping pipeline running for manual inspection");
        return false;
    }

    ++startup_retry_count_;
    std::ostringstream retry_text;
    retry_text << "Startup watchdog triggered: retry " << startup_retry_count_ << "/" << max_startup_retries_;
    std::cout << "[WARN] " << retry_text.str() << std::endl;
    logger_.log_event(std::string("VideoSession: ") + retry_text.str());

    GstStateChangeReturn shutdown_ret = gst_element_set_state(pipeline_, GST_STATE_NULL);
    GstState current_state = GST_STATE_NULL;
    GstState pending_state = GST_STATE_VOID_PENDING;
    GstStateChangeReturn wait_ret = gst_element_get_state(pipeline_, &current_state, &pending_state, 2 * GST_SECOND);

    logger_.log_event(std::string("VideoSession: startup reset state change return: ") + gst_element_state_change_return_get_name(shutdown_ret));
    std::ostringstream shutdown_text;
    shutdown_text << "VideoSession: startup reset wait result: " << gst_element_state_change_return_get_name(wait_ret)
                  << " | current=" << gst_element_state_get_name(current_state)
                  << " | pending=" << gst_element_state_get_name(pending_state);
    logger_.log_event(shutdown_text.str());

    video_sample_count_ = 0;
    missing_video_warned_ = false;
    {
        std::lock_guard<std::mutex> frame_lock(state_.frame_mutex);
        state_.current_frame.release();
        state_.new_frame_available = false;
    }

    GstStateChangeReturn play_ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    logger_.log_event(std::string("VideoSession: startup retry PLAYING request returned: ") + gst_element_state_change_return_get_name(play_ret));
    if (play_ret == GST_STATE_CHANGE_FAILURE) {
        logger_.log_event("VideoSession: startup retry failed to change pipeline state to PLAYING");
        return false;
    }

    stream_started_at_ = std::chrono::steady_clock::now();
    return true;
}

void VideoRtspSession::log_bus_message(GstMessage* msg) {
    GError* err = nullptr;
    gchar* dbg = nullptr;

    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR:
            gst_message_parse_error(msg, &err, &dbg);
            logger_.log_event(std::string("VideoSession: GStreamer error: ") + (err ? err->message : "?"));
            if (dbg) {
                logger_.log_event(std::string("VideoSession: GStreamer debug: ") + dbg);
            }
            break;
        case GST_MESSAGE_WARNING:
            gst_message_parse_warning(msg, &err, &dbg);
            logger_.log_event(std::string("VideoSession: GStreamer warning: ") + (err ? err->message : "?"));
            if (dbg) {
                logger_.log_event(std::string("VideoSession: GStreamer warning debug: ") + dbg);
            }
            break;
        case GST_MESSAGE_EOS:
            logger_.log_event("VideoSession: GStreamer EOS received");
            break;
        case GST_MESSAGE_STATE_CHANGED:
            if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline_)) {
                GstState old_state;
                GstState new_state;
                GstState pending_state;
                gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
                std::ostringstream state_text;
                state_text << "VideoSession: pipeline state changed: "
                           << gst_element_state_get_name(old_state) << " -> "
                           << gst_element_state_get_name(new_state);
                logger_.log_event(state_text.str());
            }
            break;
        case GST_MESSAGE_BUFFERING:
            if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline_)) {
                gint percent = 0;
                gst_message_parse_buffering(msg, &percent);
                logger_.log_event(std::string("VideoSession: pipeline buffering: ") + std::to_string(percent) + "%");
            }
            break;
        case GST_MESSAGE_ASYNC_DONE:
            logger_.log_event("VideoSession: pipeline async-done received");
            break;
        case GST_MESSAGE_STREAM_START:
            logger_.log_event("VideoSession: pipeline stream-start received");
            break;
        case GST_MESSAGE_LATENCY:
            logger_.log_event("VideoSession: pipeline latency message received");
            break;
        default:
            break;
    }

    if (err) {
        g_error_free(err);
    }
    if (dbg) {
        g_free(dbg);
    }
}

gboolean VideoRtspSession::on_before_send(GstElement*, GstRTSPMessage* message, gpointer user_data) {
    auto* self = static_cast<VideoRtspSession*>(user_data);
    if (message->type == GST_RTSP_MESSAGE_REQUEST) {
        GstRTSPMethod method;
        const gchar* uri = nullptr;
        if (gst_rtsp_message_parse_request(message, &method, &uri, NULL) == GST_RTSP_OK) {
            std::ostringstream rtsp_text;
            rtsp_text << "RTSP request: " << rtsp_method_label(method);
            if (uri) {
                rtsp_text << " | uri=" << uri;
            }
            std::cout << "[RTSP] " << rtsp_text.str() << std::endl;
            self->logger_.log_event(std::string("VideoSession: ") + rtsp_text.str());

            if (method == GST_RTSP_DESCRIBE || method == GST_RTSP_PLAY || method == GST_RTSP_SETUP) {
                for (const auto& kv : self->config_.headers) {
                    gst_rtsp_message_add_header_by_name(message, kv.first.c_str(), kv.second.c_str());
                    self->logger_.log_event(std::string("VideoSession: applied RTSP header: ") + kv.first);
                }
            }
        }
    }
    return TRUE;
}

void VideoRtspSession::on_src_pad_added(GstElement*, GstPad* new_pad, gpointer user_data) {
    auto* self = static_cast<VideoRtspSession*>(user_data);
    GstCaps* caps = gst_pad_get_current_caps(new_pad);
    if (!caps) {
        caps = gst_pad_query_caps(new_pad, NULL);
    }

    const std::string caps_text = self->caps_to_string(caps);
    GstStructure* structure = gst_caps_get_structure(caps, 0);
    const gchar* media = gst_structure_get_string(structure, "media");
    std::string media_str = media ? media : "unknown";
    const gchar* encoding_name = gst_structure_get_string(structure, "encoding-name");
    std::string encoding_str = encoding_name ? encoding_name : "unknown";

    std::cout << "[INFO] rtspsrc added a dynamic pad: " << media_str << " / " << encoding_str
              << " | pad=" << GST_PAD_NAME(new_pad)
              << " | caps=" << caps_text << std::endl;

    if (media_str == "video" && encoding_str == "H264") {
        GstPad* sink_pad = gst_element_get_static_pad(self->video_queue_, "sink");
        if (!gst_pad_is_linked(sink_pad)) {
            const GstPadLinkReturn link_result = gst_pad_link(new_pad, sink_pad);
            if (link_result == GST_PAD_LINK_OK) {
                std::cout << "[INFO] Linked H264 video pad to video_queue." << std::endl;
            }
        }
        gst_object_unref(sink_pad);
    }

    gst_caps_unref(caps);
}

void VideoRtspSession::on_decodebin_pad_added(GstElement*, GstPad* new_pad, gpointer user_data) {
    auto* self = static_cast<VideoRtspSession*>(user_data);
    GstPad* sink_pad = gst_element_get_static_pad(self->vconv_, "sink");
    if (gst_pad_is_linked(sink_pad)) {
        gst_object_unref(sink_pad);
        return;
    }

    GstCaps* caps = gst_pad_get_current_caps(new_pad);
    if (!caps) {
        caps = gst_pad_query_caps(new_pad, NULL);
    }

    const std::string caps_text = self->caps_to_string(caps);
    GstStructure* str = gst_caps_get_structure(caps, 0);
    const gchar* name = gst_structure_get_name(str);

    std::cout << "[INFO] decodebin pad-added: " << GST_PAD_NAME(new_pad)
              << " | caps=" << caps_text << std::endl;

    if (g_str_has_prefix(name, "video/x-raw")) {
        const GstPadLinkReturn link_result = gst_pad_link(new_pad, sink_pad);
        if (link_result == GST_PAD_LINK_OK) {
            std::cout << "[INFO] decodebin successfully linked to videoconvert." << std::endl;
        }
    }

    gst_caps_unref(caps);
    gst_object_unref(sink_pad);
}

GstFlowReturn VideoRtspSession::on_new_video_sample(GstElement* sink, gpointer user_data) {
    auto* self = static_cast<VideoRtspSession*>(user_data);
    GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
    if (!sample) {
        return GST_FLOW_ERROR;
    }

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    GstCaps* caps = gst_sample_get_caps(sample);
    GstStructure* structure = gst_caps_get_structure(caps, 0);

    int width = 0;
    int height = 0;
    gst_structure_get_int(structure, "width", &width);
    gst_structure_get_int(structure, "height", &height);

    GstMapInfo map;
    gst_buffer_map(buffer, &map, GST_MAP_READ);
    cv::Mat frame(height, width, CV_8UC3, reinterpret_cast<char*>(map.data), cv::Mat::AUTO_STEP);

    {
        std::lock_guard<std::mutex> lock(self->state_.frame_mutex);
        frame.copyTo(self->state_.current_frame);
        self->state_.new_frame_available = true;
    }

    ++self->video_sample_count_;
    if (self->video_sample_count_ == 1) {
        std::cout << "[INFO] First video sample received: " << width << "x" << height << std::endl;
        self->logger_.log_event(std::string("VideoSession: first video sample received: ") + std::to_string(width) + "x" + std::to_string(height));
        self->missing_video_warned_ = true;
    }

    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);
    return GST_FLOW_OK;
}



gboolean VideoRtspSession::on_select_stream(GstElement*, guint stream_index, GstCaps* caps, gpointer user_data) {
    auto* self = static_cast<VideoRtspSession*>(user_data);
    GstStructure* structure = gst_caps_get_structure(caps, 0);
    const gchar* media = gst_structure_get_string(structure, "media");
    const gchar* encoding_name = gst_structure_get_string(structure, "encoding-name");
    std::string media_str = media ? media : "unknown";
    std::string encoding_str = encoding_name ? encoding_name : "unknown";
    const bool select = media_str == "video";

    std::ostringstream text;
    text << "VideoSession: select-stream index=" << stream_index
         << " media=" << media_str
         << " encoding=" << encoding_str
         << " selected=" << (select ? "true" : "false");
    self->logger_.log_event(text.str());
    return select;
}
