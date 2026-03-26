#include <chrono>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/rtsp/rtsp.h>

#include "app_config.h"
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

struct ProbeOptions {
    bool include_video_track = false;
    bool use_depay = true;
    int duration_seconds = 15;
};

struct ProbeState {
    AppConfig config;
    ProbeOptions options;
    GstElement* meta_sink = nullptr;
    GstElement* meta_jitterbuffer = nullptr;
    GstElement* meta_depay = nullptr;
    int sample_count = 0;
    int object_payload_count = 0;
    int event_only_count = 0;
    int malformed_count = 0;
    bool logged_sample_caps = false;
    std::string pending_xml_fragment;
};

gboolean on_before_send(GstElement*, GstRTSPMessage* message, gpointer user_data) {
    auto* state = static_cast<ProbeState*>(user_data);
    if (message->type != GST_RTSP_MESSAGE_REQUEST) {
        return TRUE;
    }

    GstRTSPMethod method;
    const gchar* uri = nullptr;
    if (gst_rtsp_message_parse_request(message, &method, &uri, NULL) == GST_RTSP_OK) {
        std::cout << "[RTSP] " << rtsp_method_label(method);
        if (uri) {
            std::cout << " | uri=" << uri;
        }
        std::cout << std::endl;

        if (method == GST_RTSP_DESCRIBE || method == GST_RTSP_PLAY || method == GST_RTSP_SETUP) {
            for (const auto& kv : state->config.headers) {
                gst_rtsp_message_add_header_by_name(message, kv.first.c_str(), kv.second.c_str());
            }
        }
    }

    return TRUE;
}

gboolean on_select_stream(GstElement*, guint stream_index, GstCaps* caps, gpointer user_data) {
    auto* state = static_cast<ProbeState*>(user_data);
    GstStructure* structure = gst_caps_get_structure(caps, 0);
    const gchar* media = gst_structure_get_string(structure, "media");
    const gchar* encoding_name = gst_structure_get_string(structure, "encoding-name");

    std::string media_str = media ? media : "unknown";
    std::string encoding_str = encoding_name ? encoding_name : "unknown";
    const bool is_metadata = media_str == "application" &&
                             (encoding_str == "VND.ONVIF.METADATA" || encoding_str == "vnd.onvif.metadata");
    const bool is_video = media_str == "video";
    const bool select = is_metadata || (state->options.include_video_track && is_video);

    std::cout << "[SELECT] index=" << stream_index
              << " media=" << media_str
              << " encoding=" << encoding_str
              << " selected=" << (select ? "true" : "false")
              << std::endl;
    return select;
}

void on_src_pad_added(GstElement*, GstPad* new_pad, gpointer user_data) {
    auto* state = static_cast<ProbeState*>(user_data);
    GstCaps* caps = gst_pad_get_current_caps(new_pad);
    if (!caps) {
        caps = gst_pad_query_caps(new_pad, NULL);
    }

    gchar* caps_text = gst_caps_to_string(caps);
    GstStructure* structure = gst_caps_get_structure(caps, 0);
    const gchar* media = gst_structure_get_string(structure, "media");
    const gchar* encoding_name = gst_structure_get_string(structure, "encoding-name");

    std::string media_str = media ? media : "unknown";
    std::string encoding_str = encoding_name ? encoding_name : "unknown";
    std::cout << "[PAD] media=" << media_str
              << " encoding=" << encoding_str
              << " caps=" << (caps_text ? caps_text : "<none>")
              << std::endl;

    if (media_str == "application" &&
        (encoding_str == "VND.ONVIF.METADATA" || encoding_str == "vnd.onvif.metadata")) {
        GstElement* target = state->options.use_depay && state->meta_jitterbuffer ? state->meta_jitterbuffer : state->meta_sink;
        GstPad* sink_pad = gst_element_get_static_pad(target, "sink");
        if (!gst_pad_is_linked(sink_pad)) {
            const GstPadLinkReturn link_result = gst_pad_link(new_pad, sink_pad);
            std::cout << "[PAD] metadata link result=" << gst_pad_link_get_name(link_result) << std::endl;
        }
        gst_object_unref(sink_pad);
    }

    if (caps_text) {
        g_free(caps_text);
    }
    gst_caps_unref(caps);
}

GstFlowReturn on_new_meta_sample(GstElement* sink, gpointer user_data) {
    auto* state = static_cast<ProbeState*>(user_data);
    GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
    if (!sample) {
        return GST_FLOW_ERROR;
    }

    if (!state->logged_sample_caps) {
        GstCaps* sample_caps = gst_sample_get_caps(sample);
        if (sample_caps) {
            gchar* caps_text = gst_caps_to_string(sample_caps);
            std::cout << "[META] appsink-caps=" << (caps_text ? caps_text : "<none>") << std::endl;
            if (caps_text) {
                g_free(caps_text);
            }
        }
        state->logged_sample_caps = true;
    }

    ++state->sample_count;
    GstBuffer* buffer = gst_sample_get_buffer(sample);
    GstMapInfo map;
    gst_buffer_map(buffer, &map, GST_MAP_READ);

    auto process_xml = [&](const std::string& xml, bool from_pending) {
        const bool has_video_analytics = contains_video_analytics_frame(xml);
        MetadataParseResult parsed = parse_onvif_xml(xml, from_pending);
        if (!parsed.objects.empty()) {
            ++state->object_payload_count;
        } else if (!has_video_analytics) {
            ++state->event_only_count;
        }
        if (parsed.status == ParseStatus::MalformedPayload) {
            ++state->malformed_count;
            state->pending_xml_fragment = xml;
        } else if (from_pending) {
            state->pending_xml_fragment.clear();
        }

        if (state->sample_count <= 5 || !parsed.objects.empty()) {
            std::cout << "[META] sample=" << state->sample_count
                      << " status=" << parse_status_label(parsed.status)
                      << " objects=" << parsed.objects.size()
                      << " summary=" << summarize_objects(parsed.objects)
                      << std::endl;
        }
    };

    const char* xml_start = nullptr;
    for (gsize i = 0; i + 4 < map.size; ++i) {
        if (map.data[i] == '<' && map.data[i + 1] == '?' && map.data[i + 2] == 'x') {
            xml_start = reinterpret_cast<const char*>(map.data + i);
            break;
        }
    }

    if (xml_start) {
        std::string xml(xml_start, reinterpret_cast<const char*>(map.data + map.size));
        process_xml(xml, false);
    } else if (!state->pending_xml_fragment.empty()) {
        state->pending_xml_fragment.append(reinterpret_cast<const char*>(map.data), reinterpret_cast<const char*>(map.data + map.size));
        process_xml(state->pending_xml_fragment, true);
    } else {
        ++state->malformed_count;
        std::cout << "[META] sample=" << state->sample_count << " status=xml-start-not-found" << std::endl;
    }

    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

void log_bus_message(GstMessage* msg) {
    GError* err = nullptr;
    gchar* dbg = nullptr;

    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR:
            gst_message_parse_error(msg, &err, &dbg);
            std::cout << "[BUS] error=" << (err ? err->message : "?") << std::endl;
            break;
        case GST_MESSAGE_WARNING:
            gst_message_parse_warning(msg, &err, &dbg);
            std::cout << "[BUS] warning=" << (err ? err->message : "?") << std::endl;
            break;
        case GST_MESSAGE_STATE_CHANGED:
            if (GST_MESSAGE_SRC(msg)) {
                GstState old_state;
                GstState new_state;
                GstState pending_state;
                gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
                std::cout << "[BUS] state=" << gst_element_state_get_name(old_state)
                          << " -> " << gst_element_state_get_name(new_state)
                          << std::endl;
            }
            break;
        case GST_MESSAGE_STREAM_START:
            std::cout << "[BUS] stream-start" << std::endl;
            break;
        case GST_MESSAGE_EOS:
            std::cout << "[BUS] eos" << std::endl;
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

ProbeOptions parse_options(int argc, char* argv[]) {
    ProbeOptions options;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--with-video") {
            options.include_video_track = true;
        } else if (arg == "--raw-rtp") {
            options.use_depay = false;
        } else if (arg.rfind("--seconds=", 0) == 0) {
            options.duration_seconds = std::max(1, std::stoi(arg.substr(10)));
        }
    }
    return options;
}
}

int main(int argc, char* argv[]) {
    ProbeOptions options = parse_options(argc, argv);

    AppConfig config;
    std::string error_message;
    if (!load_config("config.toml", config, error_message)) {
        std::cerr << "[ERROR] " << error_message << std::endl;
        return 1;
    }

    gst_init(&argc, &argv);

    ProbeState state{config, options};
    GstElement* pipeline = gst_pipeline_new("metadata_probe_pipeline");
    GstElement* rtspsrc = gst_element_factory_make("rtspsrc", "probe_src");
    state.meta_sink = gst_element_factory_make("appsink", "probe_meta_sink");
    if (!pipeline || !rtspsrc || !state.meta_sink) {
        std::cerr << "[ERROR] Failed to create GStreamer probe elements." << std::endl;
        return 1;
    }

    if (options.use_depay) {
        state.meta_jitterbuffer = gst_element_factory_make("rtpjitterbuffer", "probe_metadata_jitterbuffer");
        state.meta_depay = gst_element_factory_make("rtponvifmetadatadepay", "probe_metadata_depay");
        if (!state.meta_jitterbuffer || !state.meta_depay) {
            std::cerr << "[ERROR] Failed to create RTP-aware ONVIF metadata depay path." << std::endl;
            return 1;
        }
    }

    g_object_set(G_OBJECT(rtspsrc),
                 "location", config.rtsp_url.c_str(),
                 "latency", config.latency,
                 "protocols", GST_RTSP_LOWER_TRANS_TCP,
                 NULL);
    g_object_set(G_OBJECT(state.meta_sink), "emit-signals", TRUE, "sync", FALSE, "async", FALSE, NULL);

    if (options.use_depay) {
        gst_bin_add_many(GST_BIN(pipeline), rtspsrc, state.meta_jitterbuffer, state.meta_depay, state.meta_sink, NULL);
        if (!gst_element_link(state.meta_jitterbuffer, state.meta_depay) || !gst_element_link(state.meta_depay, state.meta_sink)) {
            std::cerr << "[ERROR] Failed to link RTP-aware ONVIF metadata depay path." << std::endl;
            gst_object_unref(pipeline);
            return 1;
        }
    } else {
        gst_bin_add_many(GST_BIN(pipeline), rtspsrc, state.meta_sink, NULL);
    }

    g_signal_connect(rtspsrc, "before-send", G_CALLBACK(on_before_send), &state);
    g_signal_connect(rtspsrc, "select-stream", G_CALLBACK(on_select_stream), &state);
    g_signal_connect(rtspsrc, "pad-added", G_CALLBACK(on_src_pad_added), &state);
    g_signal_connect(state.meta_sink, "new-sample", G_CALLBACK(on_new_meta_sample), &state);

    std::cout << "[PROBE] url=" << config.rtsp_url << std::endl;
    std::cout << "[PROBE] mode=" << (options.include_video_track ? "metadata-with-video" : "metadata-only") << std::endl;
    std::cout << "[PROBE] metadata-path=" << (options.use_depay ? "jitterbuffer+depay" : "raw-rtp") << std::endl;
    std::cout << "[PROBE] duration=" << options.duration_seconds << "s" << std::endl;

    if (gst_element_set_state(pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "[ERROR] Failed to start metadata probe pipeline." << std::endl;
        gst_object_unref(pipeline);
        return 1;
    }

    GstBus* bus = gst_element_get_bus(pipeline);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(options.duration_seconds);
    while (std::chrono::steady_clock::now() < deadline) {
        GstMessage* msg = gst_bus_timed_pop(bus, 100 * GST_MSECOND);
        if (msg) {
            log_bus_message(msg);
            gst_message_unref(msg);
        }
    }

    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    std::cout << "[SUMMARY] metadata_samples=" << state.sample_count
              << " object_payloads=" << state.object_payload_count
              << " event_only=" << state.event_only_count
              << " malformed=" << state.malformed_count
              << std::endl;
    return 0;
}
