#include "metadata_rtsp_session.h"

#include <chrono>
#include <sstream>

#include <gst/app/gstappsink.h>
#include <gst/rtsp/rtsp.h>

#include "metadata_parser.h"

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

MetadataRtspSession::MetadataRtspSession(const AppConfig& config, SessionLogger& logger, SharedAppState& state)
    : config_(config), logger_(logger), state_(state), enabled_(config.enable_metadata) {}

MetadataRtspSession::~MetadataRtspSession() {
    stop();
}

std::string MetadataRtspSession::caps_to_string(GstCaps* caps) const {
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

bool MetadataRtspSession::start() {
    if (!enabled_) {
        logger_.log_event("MetadataSession: disabled by config; skipping metadata RTSP session");
        return true;
    }

    pipeline_ = gst_pipeline_new("metadata_pipeline");
    GstElement* rtspsrc = gst_element_factory_make("rtspsrc", "metadata_src");
    meta_sink_ = gst_element_factory_make("appsink", "meta_sink");

    if (!pipeline_ || !rtspsrc || !meta_sink_) {
        logger_.log_event("MetadataSession: failed to create GStreamer elements");
        return false;
    }

    g_object_set(G_OBJECT(rtspsrc),
                 "location", config_.rtsp_url.c_str(),
                 "latency", config_.latency,
                 "protocols", GST_RTSP_LOWER_TRANS_TCP,
                 NULL);
    g_signal_connect(rtspsrc, "before-send", G_CALLBACK(MetadataRtspSession::on_before_send), this);
    g_signal_connect(rtspsrc, "select-stream", G_CALLBACK(MetadataRtspSession::on_select_stream), this);

    g_object_set(G_OBJECT(meta_sink_), "emit-signals", TRUE, "sync", FALSE, "async", FALSE, NULL);

    gst_bin_add_many(GST_BIN(pipeline_), rtspsrc, meta_sink_, NULL);
    g_signal_connect(rtspsrc, "pad-added", G_CALLBACK(MetadataRtspSession::on_src_pad_added), this);
    g_signal_connect(meta_sink_, "new-sample", G_CALLBACK(MetadataRtspSession::on_new_meta_sample), this);

    logger_.log_event("MetadataSession: starting pipeline");
    GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        logger_.log_event("MetadataSession: failed to change pipeline state to PLAYING");
        return false;
    }

    return true;
}

void MetadataRtspSession::stop() {
    if (!pipeline_) {
        return;
    }

    logger_.log_event("MetadataSession: requesting pipeline shutdown (GST_STATE_NULL)");
    GstStateChangeReturn shutdown_ret = gst_element_set_state(pipeline_, GST_STATE_NULL);
    logger_.log_event(std::string("MetadataSession: shutdown state change return: ") + gst_element_state_change_return_get_name(shutdown_ret));

    GstState current_state = GST_STATE_VOID_PENDING;
    GstState pending_state = GST_STATE_VOID_PENDING;
    GstStateChangeReturn wait_ret = gst_element_get_state(pipeline_, &current_state, &pending_state, 2 * GST_SECOND);
    std::ostringstream shutdown_text;
    shutdown_text << "MetadataSession: shutdown wait result: " << gst_element_state_change_return_get_name(wait_ret)
                  << " | current=" << gst_element_state_get_name(current_state)
                  << " | pending=" << gst_element_state_get_name(pending_state);
    logger_.log_event(shutdown_text.str());

    gst_object_unref(pipeline_);
    pipeline_ = nullptr;
    meta_sink_ = nullptr;
}

void MetadataRtspSession::poll_bus_once(GstClockTime timeout) {
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
}

void MetadataRtspSession::log_bus_message(GstMessage* msg) {
    GError* err = nullptr;
    gchar* dbg = nullptr;

    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR:
            gst_message_parse_error(msg, &err, &dbg);
            logger_.log_event(std::string("MetadataSession: GStreamer error: ") + (err ? err->message : "?"));
            if (dbg) {
                logger_.log_event(std::string("MetadataSession: GStreamer debug: ") + dbg);
            }
            break;
        case GST_MESSAGE_WARNING:
            gst_message_parse_warning(msg, &err, &dbg);
            logger_.log_event(std::string("MetadataSession: GStreamer warning: ") + (err ? err->message : "?"));
            if (dbg) {
                logger_.log_event(std::string("MetadataSession: GStreamer warning debug: ") + dbg);
            }
            break;
        case GST_MESSAGE_EOS:
            logger_.log_event("MetadataSession: GStreamer EOS received");
            break;
        case GST_MESSAGE_STATE_CHANGED:
            if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline_)) {
                GstState old_state;
                GstState new_state;
                GstState pending_state;
                gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
                std::ostringstream state_text;
                state_text << "MetadataSession: pipeline state changed: "
                           << gst_element_state_get_name(old_state) << " -> "
                           << gst_element_state_get_name(new_state);
                logger_.log_event(state_text.str());
            }
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

gboolean MetadataRtspSession::on_before_send(GstElement*, GstRTSPMessage* message, gpointer user_data) {
    auto* self = static_cast<MetadataRtspSession*>(user_data);
    if (message->type == GST_RTSP_MESSAGE_REQUEST) {
        GstRTSPMethod method;
        const gchar* uri = nullptr;
        if (gst_rtsp_message_parse_request(message, &method, &uri, NULL) == GST_RTSP_OK) {
            std::ostringstream rtsp_text;
            rtsp_text << "RTSP request: " << rtsp_method_label(method);
            if (uri) {
                rtsp_text << " | uri=" << uri;
            }
            self->logger_.log_event(std::string("MetadataSession: ") + rtsp_text.str());

            if (method == GST_RTSP_DESCRIBE || method == GST_RTSP_PLAY || method == GST_RTSP_SETUP) {
                for (const auto& kv : self->config_.headers) {
                    gst_rtsp_message_add_header_by_name(message, kv.first.c_str(), kv.second.c_str());
                    self->logger_.log_event(std::string("MetadataSession: applied RTSP header: ") + kv.first);
                }
            }
        }
    }
    return TRUE;
}

void MetadataRtspSession::on_src_pad_added(GstElement*, GstPad* new_pad, gpointer user_data) {
    auto* self = static_cast<MetadataRtspSession*>(user_data);
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

    if (media_str == "application" &&
        (encoding_str == "vnd.onvif.metadata" || encoding_str == "VND.ONVIF.METADATA")) {
        GstPad* sink_pad = gst_element_get_static_pad(self->meta_sink_, "sink");
        if (!gst_pad_is_linked(sink_pad)) {
            GstPadLinkReturn link_result = gst_pad_link(new_pad, sink_pad);
            if (link_result == GST_PAD_LINK_OK) {
                self->logger_.log_event(std::string("MetadataSession: linked metadata pad | caps=") + caps_text);
            }
        }
        gst_object_unref(sink_pad);
    }

    gst_caps_unref(caps);
}

GstFlowReturn MetadataRtspSession::on_new_meta_sample(GstElement* sink, gpointer user_data) {
    auto* self = static_cast<MetadataRtspSession*>(user_data);
    GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
    if (!sample) {
        return GST_FLOW_ERROR;
    }

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    GstMapInfo map;
    gst_buffer_map(buffer, &map, GST_MAP_READ);

    const char* xml_start = nullptr;
    for (gsize i = 0; i + 4 < map.size; ++i) {
        if (map.data[i] == '<' && map.data[i + 1] == '?' && map.data[i + 2] == 'x') {
            xml_start = reinterpret_cast<const char*>(map.data + i);
            break;
        }
    }

    if (xml_start) {
        std::string xml_str(xml_start, reinterpret_cast<const char*>(map.data + map.size));
        self->logger_.log_raw_metadata(xml_str);

        if (self->config_.capture_fixture_candidates) {
            std::string saved_path;
            if (self->logger_.capture_fixture_candidate(self->config_.fixture_output_dir, self->config_.fixture_sample_limit, xml_str, saved_path)) {
                self->logger_.log_event(std::string("MetadataSession: saved fixture candidate: ") + saved_path);
            }
        }

        const bool has_video_analytics = contains_video_analytics_frame(xml_str);
        const bool has_objects = contains_object_blocks(xml_str);

        MetadataParseResult parse_result = parse_onvif_xml(xml_str);
        std::string summary = std::string("status=") + parse_status_label(parse_result.status) +
                              " message=\"" + parse_result.message + "\" " +
                              summarize_objects(parse_result.objects);
        self->logger_.log_parsed_summary(summary);
        self->logger_.log_event(std::string("MetadataSession: parse result: ") + summary);

        if (has_video_analytics) {
            self->state_.last_parse_status_text = std::string("Parse: ") + parse_status_label(parse_result.status) +
                                                  " | objects=" + std::to_string(parse_result.objects.size());

            std::lock_guard<std::mutex> lock(self->state_.meta_mutex);
            if (has_objects &&
                (parse_result.status == ParseStatus::Success || parse_result.status == ParseStatus::UnknownPattern)) {
                self->state_.current_objects = std::move(parse_result.objects);
                self->state_.last_metadata_update = std::chrono::steady_clock::now();
                self->state_.has_metadata_update = true;
            } else if (!has_objects && parse_result.status == ParseStatus::NoObjects) {
                self->state_.current_objects.clear();
                self->state_.last_metadata_update = std::chrono::steady_clock::now();
                self->state_.has_metadata_update = true;
            }
        } else {
            self->state_.last_parse_status_text = "Parse: event-only | overlay unchanged";
            self->logger_.log_event("MetadataSession: event-only metadata ignored for overlay state");
        }
    } else {
        self->logger_.log_event("MetadataSession: metadata payload received without XML start marker");
        self->logger_.log_parsed_summary("status=malformed-payload message=\"XML start marker not found\" objects=0");
        self->state_.last_parse_status_text = "Parse: malformed-payload | objects=0";
    }

    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);
    return GST_FLOW_OK;
}


gboolean MetadataRtspSession::on_select_stream(GstElement*, guint stream_index, GstCaps* caps, gpointer user_data) {
    auto* self = static_cast<MetadataRtspSession*>(user_data);
    GstStructure* structure = gst_caps_get_structure(caps, 0);
    const gchar* media = gst_structure_get_string(structure, "media");
    const gchar* encoding_name = gst_structure_get_string(structure, "encoding-name");
    std::string media_str = media ? media : "unknown";
    std::string encoding_str = encoding_name ? encoding_name : "unknown";
    const bool is_metadata = media_str == "application" &&
                             (encoding_str == "VND.ONVIF.METADATA" || encoding_str == "vnd.onvif.metadata");
    const bool is_video = media_str == "video";
    const bool select = is_video || is_metadata;

    std::ostringstream text;
    text << "MetadataSession: select-stream index=" << stream_index
         << " media=" << media_str
         << " encoding=" << encoding_str
         << " selected=" << (select ? "true" : "false");
    self->logger_.log_event(text.str());
    return select;
}


