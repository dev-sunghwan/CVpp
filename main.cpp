#include <iostream>
#include <sstream>
#include <string>
#include <mutex>
#include <vector>
#include <regex>
#include <chrono>
#include <map>
#include <atomic>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/rtsp/rtsp.h>
#include <opencv2/opencv.hpp>

#include "app_config.h"
#include "metadata_types.h"
#include "session_logger.h"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <conio.h>
#endif

cv::Mat current_frame;
std::vector<DetectedObject> current_objects;
std::mutex frame_mutex;
std::mutex meta_mutex;
bool new_frame_available = false;
std::chrono::steady_clock::time_point last_metadata_update;
bool has_metadata_update = false;
std::string last_parse_status_text = "No metadata parsed yet";
SessionLogger g_logger;
int g_video_sample_count = 0;
bool g_missing_video_warned = false;
std::atomic<bool> g_shutdown_requested = false;
bool g_enable_metadata = true;
int g_startup_retry_count = 0;

static std::string caps_to_string(GstCaps* caps) {
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

static const char* rtsp_method_label(GstRTSPMethod method) {
    switch (method) {
        case GST_RTSP_OPTIONS: return "OPTIONS";
        case GST_RTSP_DESCRIBE: return "DESCRIBE";
        case GST_RTSP_ANNOUNCE: return "ANNOUNCE";
        case GST_RTSP_GET_PARAMETER: return "GET_PARAMETER";
        case GST_RTSP_PAUSE: return "PAUSE";
        case GST_RTSP_PLAY: return "PLAY";
        case GST_RTSP_RECORD: return "RECORD";
        case GST_RTSP_REDIRECT: return "REDIRECT";
        case GST_RTSP_SETUP: return "SETUP";
        case GST_RTSP_SET_PARAMETER: return "SET_PARAMETER";
        case GST_RTSP_TEARDOWN: return "TEARDOWN";
        default: return "UNKNOWN";
    }
}
#ifdef _WIN32
static BOOL WINAPI console_ctrl_handler(DWORD ctrl_type) {
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
static const char* parse_status_label(ParseStatus status) {
    switch (status) {
        case ParseStatus::Success:
            return "success";
        case ParseStatus::UnknownPattern:
            return "unknown-pattern";
        case ParseStatus::NoObjects:
            return "no-objects";
        case ParseStatus::MalformedPayload:
        default:
            return "malformed-payload";
    }
}

static std::string summarize_objects(const std::vector<DetectedObject>& objects) {
    if (objects.empty()) {
        return "objects=0";
    }

    std::ostringstream summary;
    summary << "objects=" << objects.size() << " ";
    for (size_t i = 0; i < objects.size(); ++i) {
        const auto& obj = objects[i];
        if (i > 0) {
            summary << "; ";
        }
        summary << "id=" << obj.id
                << ",type=" << obj.type
                << ",score=" << static_cast<int>(obj.likelihood * 100) << "%";
    }
    return summary.str();
}

static MetadataParseResult parse_onvif_xml(const std::string& xml) {
    MetadataParseResult result;

    const std::string start_tag = "<tt:Object ObjectId=";
    const std::string end_tag = "</tt:Object>";

    if (xml.find("<?xml") == std::string::npos) {
        result.status = ParseStatus::MalformedPayload;
        result.message = "XML declaration not found";
        return result;
    }

    std::regex id_re("ObjectId=\"(\\d+)\"");
    std::regex bbox_re("<tt:BoundingBox left=\"([0-9.-]+)\" top=\"([0-9.-]+)\" right=\"([0-9.-]+)\" bottom=\"([0-9.-]+)\"");
    std::regex class_type_re("<tt:Class>[\\s\\S]*?<tt:Type Likelihood=\"([0-9.-]+)\">([^<]+)</tt:Type>[\\s\\S]*?</tt:Class>");
    std::regex candidate_re("<tt:ClassCandidate>[\\s\\S]*?<tt:Type>([^<]+)</tt:Type>[\\s\\S]*?<tt:Likelihood>([0-9.-]+)</tt:Likelihood>[\\s\\S]*?</tt:ClassCandidate>");
    std::regex vehicle_re("<tt:VehicleInfo>[\\s\\S]*?<tt:Type Likelihood=\"([0-9.-]+)\">([^<]+)</tt:Type>[\\s\\S]*?</tt:VehicleInfo>");
    std::regex human_re("<tt:HumanInfo>[\\s\\S]*?<tt:Type Likelihood=\"([0-9.-]+)\">([^<]+)</tt:Type>[\\s\\S]*?</tt:HumanInfo>");

    bool found_object_block = false;
    bool found_unknown_pattern = false;
    size_t pos = 0;
    while ((pos = xml.find(start_tag, pos)) != std::string::npos) {
        size_t end = xml.find(end_tag, pos);
        if (end == std::string::npos) {
            result.status = ParseStatus::MalformedPayload;
            result.message = "Object block did not close cleanly";
            return result;
        }

        found_object_block = true;
        end += end_tag.size();
        std::string block = xml.substr(pos, end - pos);
        pos = end;

        DetectedObject obj;
        bool has_any_detail = false;
        std::smatch id_m;
        if (std::regex_search(block, id_m, id_re)) {
            obj.id = std::stoi(id_m[1].str());
        }

        std::smatch bbox_m;
        if (std::regex_search(block, bbox_m, bbox_re)) {
            obj.left = std::stof(bbox_m[1].str());
            obj.top = std::stof(bbox_m[2].str());
            obj.right = std::stof(bbox_m[3].str());
            obj.bottom = std::stof(bbox_m[4].str());
        }

        std::smatch detail_m;
        if (std::regex_search(block, detail_m, vehicle_re) || std::regex_search(block, detail_m, human_re)) {
            obj.likelihood = std::stof(detail_m[1].str());
            obj.type = detail_m[2].str();
            has_any_detail = true;
        } else {
            std::smatch class_m;
            if (std::regex_search(block, class_m, class_type_re)) {
                obj.likelihood = std::stof(class_m[1].str());
                obj.type = class_m[2].str();
                has_any_detail = true;
            } else {
                std::smatch candidate_m;
                if (std::regex_search(block, candidate_m, candidate_re)) {
                    obj.type = candidate_m[1].str();
                    obj.likelihood = std::stof(candidate_m[2].str());
                    has_any_detail = true;
                }
            }
        }

        if (!has_any_detail) {
            found_unknown_pattern = true;
        }

        result.objects.push_back(obj);
    }

    if (!found_object_block) {
        result.status = ParseStatus::NoObjects;
        result.message = "No <tt:Object> blocks found";
        return result;
    }

    if (result.objects.empty()) {
        result.status = ParseStatus::NoObjects;
        result.message = "No objects parsed";
        return result;
    }

    result.status = found_unknown_pattern ? ParseStatus::UnknownPattern : ParseStatus::Success;
    result.message = found_unknown_pattern ? "Objects parsed but some class patterns were unknown" : "Objects parsed successfully";
    return result;
}

static gboolean on_before_send(GstElement*, GstRTSPMessage* message, gpointer user_data) {
    auto* custom_headers = static_cast<std::map<std::string, std::string>*>(user_data);

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
            g_logger.log_event(rtsp_text.str());

            if (custom_headers && (method == GST_RTSP_DESCRIBE || method == GST_RTSP_PLAY || method == GST_RTSP_SETUP)) {
                for (const auto& kv : *custom_headers) {
                    gst_rtsp_message_add_header_by_name(message, kv.first.c_str(), kv.second.c_str());
                    g_logger.log_event(std::string("Applied RTSP header: ") + kv.first);
                }
            }
        }
    }
    return TRUE;
}

static void on_decodebin_pad_added(GstElement*, GstPad* new_pad, gpointer user_data) {
    GstElement* vconv = GST_ELEMENT(user_data);
    GstPad* sink_pad = gst_element_get_static_pad(vconv, "sink");
    if (gst_pad_is_linked(sink_pad)) {
        gst_object_unref(sink_pad);
        return;
    }

    GstCaps* caps = gst_pad_get_current_caps(new_pad);
    if (!caps) {
        caps = gst_pad_query_caps(new_pad, NULL);
    }

    const std::string caps_text = caps_to_string(caps);
    GstStructure* str = gst_caps_get_structure(caps, 0);
    const gchar* name = gst_structure_get_name(str);

    std::cout << "[INFO] decodebin pad-added: " << GST_PAD_NAME(new_pad)
              << " | caps=" << caps_text << std::endl;

    if (g_str_has_prefix(name, "video/x-raw")) {
        const GstPadLinkReturn link_result = gst_pad_link(new_pad, sink_pad);
        if (link_result == GST_PAD_LINK_OK) {
            std::cout << "[INFO] decodebin successfully linked to videoconvert." << std::endl;
        } else {
            std::cout << "[WARN] Failed to link decodebin pad to videoconvert: "
                      << gst_pad_link_get_name(link_result) << std::endl;
        }
    }

    gst_caps_unref(caps);
    gst_object_unref(sink_pad);
}

static void on_src_pad_added(GstElement*, GstPad* new_pad, gpointer user_data) {
    GstElement* pipeline = GST_ELEMENT(user_data);
    GstCaps* caps = gst_pad_get_current_caps(new_pad);
    if (!caps) {
        caps = gst_pad_query_caps(new_pad, NULL);
    }

    const std::string caps_text = caps_to_string(caps);
    GstStructure* structure = gst_caps_get_structure(caps, 0);
    const gchar* media = gst_structure_get_string(structure, "media");
    std::string media_str = media ? media : "unknown";
    const gchar* encoding_name = gst_structure_get_string(structure, "encoding-name");
    std::string encoding_str = encoding_name ? encoding_name : "unknown";

    std::cout << "[INFO] rtspsrc added a dynamic pad: " << media_str << " / " << encoding_str
              << " | pad=" << GST_PAD_NAME(new_pad)
              << " | caps=" << caps_text << std::endl;

    if (media_str == "video" && encoding_str == "H264") {
        GstElement* video_queue = gst_bin_get_by_name(GST_BIN(pipeline), "video_queue");
        GstPad* sink_pad = gst_element_get_static_pad(video_queue, "sink");
        if (!gst_pad_is_linked(sink_pad)) {
            const GstPadLinkReturn link_result = gst_pad_link(new_pad, sink_pad);
            if (link_result == GST_PAD_LINK_OK) {
                std::cout << "[INFO] Linked H264 video pad to video_queue." << std::endl;
            } else {
                std::cout << "[WARN] Failed to link H264 video pad to video_queue: "
                          << gst_pad_link_get_name(link_result) << std::endl;
            }
        } else {
            std::cout << "[INFO] video_queue sink already linked, ignoring extra video pad." << std::endl;
        }
        gst_object_unref(sink_pad);
        gst_object_unref(video_queue);
    } else if (media_str == "video" && (encoding_str == "JPEG" || encoding_str == "MJPEG")) {
        std::cout << "[WARN] JPEG/MJPEG video is not handled in explicit H264 mode." << std::endl;
    } else if (media_str == "application" &&
               (encoding_str == "vnd.onvif.metadata" || encoding_str == "VND.ONVIF.METADATA")) {
        if (!g_enable_metadata) {
            std::cout << "[INFO] Metadata branch disabled by config. Ignoring ONVIF metadata pad." << std::endl;
            gst_caps_unref(caps);
            return;
        }
        GstElement* meta_sink = gst_bin_get_by_name(GST_BIN(pipeline), "meta_sink");
        GstPad* sink_pad = gst_element_get_static_pad(meta_sink, "sink");
        const GstPadLinkReturn link_result = gst_pad_link(new_pad, sink_pad);
        if (link_result == GST_PAD_LINK_OK) {
            std::cout << "[INFO] Linked ONVIF metadata pad to meta_sink." << std::endl;
        } else {
            std::cout << "[WARN] Failed to link ONVIF metadata pad: "
                      << gst_pad_link_get_name(link_result) << std::endl;
        }
        gst_object_unref(sink_pad);
        gst_object_unref(meta_sink);
    } else {
        std::cout << "[INFO] Ignoring pad: " << media_str << "/" << encoding_str << std::endl;
    }

    gst_caps_unref(caps);
}

static GstFlowReturn on_new_meta_sample(GstElement* sink, gpointer user_data) {
    auto* config = static_cast<AppConfig*>(user_data);
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
        g_logger.log_raw_metadata(xml_str);

        if (config && config->capture_fixture_candidates) {
            std::string saved_path;
            if (g_logger.capture_fixture_candidate(config->fixture_output_dir, config->fixture_sample_limit, xml_str, saved_path)) {
                g_logger.log_event(std::string("Saved fixture candidate: ") + saved_path);
            }
        }

        MetadataParseResult parse_result = parse_onvif_xml(xml_str);
        std::string summary = std::string("status=") + parse_status_label(parse_result.status) +
                              " message=\"" + parse_result.message + "\" " +
                              summarize_objects(parse_result.objects);
        g_logger.log_parsed_summary(summary);
        g_logger.log_event(std::string("Metadata parse result: ") + summary);
        last_parse_status_text = std::string("Parse: ") + parse_status_label(parse_result.status) +
                                 " | objects=" + std::to_string(parse_result.objects.size());

        if (parse_result.status == ParseStatus::Success ||
            parse_result.status == ParseStatus::UnknownPattern ||
            parse_result.status == ParseStatus::NoObjects) {
            std::lock_guard<std::mutex> lock(meta_mutex);
            current_objects = std::move(parse_result.objects);
            last_metadata_update = std::chrono::steady_clock::now();
            has_metadata_update = true;
        }
    } else {
        g_logger.log_event("Metadata payload received without XML start marker");
        g_logger.log_parsed_summary("status=malformed-payload message=\"XML start marker not found\" objects=0");
        last_parse_status_text = "Parse: malformed-payload | objects=0";
    }

    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

static GstFlowReturn on_new_video_sample(GstElement* sink, gpointer) {
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
        std::lock_guard<std::mutex> lock(frame_mutex);
        frame.copyTo(current_frame);
        new_frame_available = true;
    }

    ++g_video_sample_count;
    if (g_video_sample_count == 1) {
        std::cout << "[INFO] First video sample received: " << width << "x" << height << std::endl;
        g_logger.log_event(std::string("First video sample received: ") + std::to_string(width) + "x" + std::to_string(height));
        g_missing_video_warned = true;
    }

    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

static bool restart_pipeline_after_startup_timeout(GstElement* pipeline, int max_startup_retries, std::chrono::steady_clock::time_point& stream_started_at) {
    if (g_startup_retry_count >= max_startup_retries) {
        return false;
    }

    ++g_startup_retry_count;
    std::ostringstream retry_text;
    retry_text << "Startup watchdog triggered: retry " << g_startup_retry_count << "/" << max_startup_retries;
    std::cout << "[WARN] " << retry_text.str() << std::endl;
    g_logger.log_event(retry_text.str());

    g_logger.log_event("Startup watchdog requesting pipeline reset to NULL");
    GstStateChangeReturn shutdown_ret = gst_element_set_state(pipeline, GST_STATE_NULL);
    GstState current_state = GST_STATE_NULL;
    GstState pending_state = GST_STATE_VOID_PENDING;
    GstStateChangeReturn wait_ret = gst_element_get_state(pipeline, &current_state, &pending_state, 2 * GST_SECOND);

    std::ostringstream shutdown_text;
    shutdown_text << "Startup reset wait result: " << gst_element_state_change_return_get_name(wait_ret)
                  << " | current=" << gst_element_state_get_name(current_state)
                  << " | pending=" << gst_element_state_get_name(pending_state);
    g_logger.log_event(std::string("Startup reset state change return: ") + gst_element_state_change_return_get_name(shutdown_ret));
    g_logger.log_event(shutdown_text.str());

    g_video_sample_count = 0;
    g_missing_video_warned = false;
    has_metadata_update = false;
    {
        std::lock_guard<std::mutex> frame_lock(frame_mutex);
        current_frame.release();
        new_frame_available = false;
    }
    {
        std::lock_guard<std::mutex> meta_lock(meta_mutex);
        current_objects.clear();
    }
    last_parse_status_text = "Restarting stream after startup timeout";

    GstStateChangeReturn play_ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    g_logger.log_event(std::string("Startup retry PLAYING request returned: ") + gst_element_state_change_return_get_name(play_ret));
    if (play_ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "[ERROR] Startup retry failed to change pipeline state to PLAYING." << std::endl;
        g_logger.log_event("Startup retry failed to change pipeline state to PLAYING");
        return false;
    }

    stream_started_at = std::chrono::steady_clock::now();
    return true;
}

static void draw_overlay(cv::Mat& frame, const std::vector<DetectedObject>& objects) {
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

static void draw_status_banner(cv::Mat& frame, const std::string& status_text) {
    cv::rectangle(frame, cv::Point(10, 10), cv::Point(frame.cols - 10, 40), cv::Scalar(20, 20, 20), cv::FILLED);
    cv::putText(frame, status_text, cv::Point(20, 33), cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(0, 255, 255), 1);
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

    if (!g_logger.initialize(config.output_root, error_message)) {
        std::cerr << "[ERROR] Failed to initialize logging: " << error_message << std::endl;
        return 1;
    }

    std::cout << "[INFO] Session output directory: " << g_logger.session_dir() << std::endl;
    g_enable_metadata = config.enable_metadata;
    g_logger.log_event(std::string("Metadata branch: ") + (config.enable_metadata ? "enabled" : "disabled"));
    if (!config.enable_metadata) {
        g_logger.log_event("Metadata appsink is not created in video-only mode");
    }
    g_logger.log_event("Config loaded from config.toml");
    g_logger.log_event(std::string("Fixture candidate capture: ") + (config.capture_fixture_candidates ? "enabled" : "disabled"));

#ifdef _WIN32
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
#endif

    gst_init(&argc, &argv);
    std::cout << "[INFO] GStreamer core engine initialized." << std::endl;

    GstElement* pipeline = gst_pipeline_new("analytics_pipeline");
    GstElement* rtspsrc = gst_element_factory_make("rtspsrc", "mysrc");
    GstElement* video_queue = gst_element_factory_make("queue", "video_queue");
    GstElement* h264_depay = gst_element_factory_make("rtph264depay", "h264_depay");
    GstElement* h264_parse = gst_element_factory_make("h264parse", "h264_parse");
    GstElement* decodebin = gst_element_factory_make("decodebin", "decodebin");
    GstElement* vconv = gst_element_factory_make("videoconvert", "vconv");
    GstElement* video_sink = gst_element_factory_make("appsink", "video_sink");
    GstElement* meta_sink = config.enable_metadata ? gst_element_factory_make("appsink", "meta_sink") : nullptr;

    if (!pipeline || !rtspsrc || !video_queue || !h264_depay || !h264_parse || !decodebin || !vconv || !video_sink || (config.enable_metadata && !meta_sink)) {
        std::cerr << "[ERROR] Failed to create GStreamer elements." << std::endl;
        g_logger.log_event("Failed to create GStreamer elements");
        return 1;
    }

    g_object_set(G_OBJECT(rtspsrc),
                 "location", config.rtsp_url.c_str(),
                 "latency", config.latency,
                 "protocols", GST_RTSP_LOWER_TRANS_TCP,
                 NULL);
    g_signal_connect(rtspsrc, "before-send", G_CALLBACK(on_before_send), &config.headers);

    GstCaps* caps_v = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "BGR", NULL);
    g_object_set(G_OBJECT(video_sink), "caps", caps_v, "emit-signals", TRUE, "sync", FALSE,
                 "max-buffers", 2, "drop", TRUE, NULL);
    gst_caps_unref(caps_v);

    if (meta_sink) {
        g_object_set(G_OBJECT(meta_sink), "emit-signals", TRUE, "sync", FALSE, "async", FALSE, NULL);
    }
    g_object_set(G_OBJECT(video_queue), "leaky", 2, "max-size-buffers", 8, NULL);

    if (meta_sink) {
        gst_bin_add_many(GST_BIN(pipeline), rtspsrc, video_queue, h264_depay, h264_parse, decodebin, vconv, video_sink, meta_sink, NULL);
    } else {
        gst_bin_add_many(GST_BIN(pipeline), rtspsrc, video_queue, h264_depay, h264_parse, decodebin, vconv, video_sink, NULL);
    }

    if (!gst_element_link_many(video_queue, h264_depay, h264_parse, decodebin, NULL)) {
        std::cerr << "[ERROR] Failed to link explicit H264 path to decodebin." << std::endl;
        g_logger.log_event("Failed to link explicit H264 path to decodebin");
        return 1;
    }

    if (!gst_element_link(vconv, video_sink)) {
        std::cerr << "[ERROR] Failed to link vconv to video_sink." << std::endl;
        g_logger.log_event("Failed to link vconv to video_sink");
        return 1;
    }

    g_signal_connect(rtspsrc, "pad-added", G_CALLBACK(on_src_pad_added), pipeline);
    g_signal_connect(decodebin, "pad-added", G_CALLBACK(on_decodebin_pad_added), vconv);
    g_signal_connect(video_sink, "new-sample", G_CALLBACK(on_new_video_sample), nullptr);
    if (meta_sink) {
        g_signal_connect(meta_sink, "new-sample", G_CALLBACK(on_new_meta_sample), &config);
    }

    std::cout << "[INFO] Starting pipeline..." << std::endl;
    g_logger.log_event("Starting pipeline");
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "[ERROR] Failed to change pipeline state to PLAYING." << std::endl;
        g_logger.log_event("Failed to change pipeline state to PLAYING");
        gst_object_unref(pipeline);
        return 1;
    }
    std::cout << "[INFO] Streaming. Press ESC to exit." << std::endl;

    auto stream_started_at = std::chrono::steady_clock::now();
    const int max_startup_retries = 2;

    while (true) {
        GstBus* bus = gst_element_get_bus(pipeline);
        GstMessage* msg = gst_bus_timed_pop(bus, 10 * GST_MSECOND);
        if (msg) {
            GError* err = nullptr;
            gchar* dbg = nullptr;
            switch (GST_MESSAGE_TYPE(msg)) {
                case GST_MESSAGE_ERROR:
                    gst_message_parse_error(msg, &err, &dbg);
                    std::cerr << "[GST ERROR] " << (err ? err->message : "?") << std::endl;
                    g_logger.log_event(std::string("GStreamer error: ") + (err ? err->message : "?"));
                    if (dbg) {
                        std::cerr << "[GST DEBUG] " << dbg << std::endl;
                        g_logger.log_event(std::string("GStreamer debug: ") + dbg);
                    }
                    if (err) {
                        g_error_free(err);
                    }
                    if (dbg) {
                        g_free(dbg);
                    }
                    break;
                case GST_MESSAGE_WARNING:
                    gst_message_parse_warning(msg, &err, &dbg);
                    std::cerr << "[GST WARN]  " << (err ? err->message : "?") << std::endl;
                    g_logger.log_event(std::string("GStreamer warning: ") + (err ? err->message : "?"));
                    if (dbg) {
                        g_logger.log_event(std::string("GStreamer warning debug: ") + dbg);
                    }
                    if (err) {
                        g_error_free(err);
                    }
                    if (dbg) {
                        g_free(dbg);
                    }
                    break;
                case GST_MESSAGE_EOS:
                    std::cout << "[GST INFO] End of stream received." << std::endl;
                    g_logger.log_event("GStreamer EOS received");
                    break;
                case GST_MESSAGE_STATE_CHANGED:
                    if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline)) {
                        GstState old_state;
                        GstState new_state;
                        GstState pending_state;
                        gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
                        std::ostringstream state_text;
                        state_text << "Pipeline state changed: "
                                   << gst_element_state_get_name(old_state) << " -> "
                                   << gst_element_state_get_name(new_state);
                        g_logger.log_event(state_text.str());
                    }
                    break;
                case GST_MESSAGE_BUFFERING:
                    if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline)) {
                        gint percent = 0;
                        gst_message_parse_buffering(msg, &percent);
                        g_logger.log_event(std::string("Pipeline buffering: ") + std::to_string(percent) + "%");
                    }
                    break;
                case GST_MESSAGE_ASYNC_DONE:
                    g_logger.log_event("Pipeline async-done received");
                    break;
                case GST_MESSAGE_STREAM_START:
                    g_logger.log_event("Pipeline stream-start received");
                    break;
                case GST_MESSAGE_LATENCY:
                    g_logger.log_event("Pipeline latency message received");
                    break;
                default:
                    break;
            }
            gst_message_unref(msg);
        }
        gst_object_unref(bus);

        const auto now = std::chrono::steady_clock::now();
        if (!g_missing_video_warned && g_video_sample_count == 0 &&
            std::chrono::duration_cast<std::chrono::seconds>(now - stream_started_at).count() >= 10) {
            std::cout << "[WARN] No video samples received within 10 seconds." << std::endl;
            g_logger.log_event("No video samples received within 10 seconds");
            g_missing_video_warned = true;

            if (restart_pipeline_after_startup_timeout(pipeline, max_startup_retries, stream_started_at)) {
                continue;
            }

            g_logger.log_event("Startup watchdog retries exhausted; keeping pipeline running for manual inspection");
        }

        cv::Mat display_frame;
        {
            std::lock_guard<std::mutex> lock(frame_mutex);
            if (new_frame_available) {
                display_frame = current_frame.clone();
                new_frame_available = false;
            }
        }

        if (!display_frame.empty()) {
            std::vector<DetectedObject> overlay_objects;
            {
                std::lock_guard<std::mutex> lock(meta_mutex);
                const auto fresh_now = std::chrono::steady_clock::now();
                const bool metadata_is_fresh =
                    has_metadata_update &&
                    (std::chrono::duration_cast<std::chrono::milliseconds>(fresh_now - last_metadata_update).count() <= 500);

                if (metadata_is_fresh) {
                    overlay_objects = current_objects;
                }
            }
            if (!overlay_objects.empty()) {
                draw_overlay(display_frame, overlay_objects);
            }
            draw_status_banner(display_frame, last_parse_status_text);
            cv::imshow("GStreamer Analytics (ESC to quit)", display_frame);
        }

        const int window_key = cv::waitKey(30);
        if (window_key == 27) {
            std::cout << "\n[MAIN] ESC pressed in display window. Shutting down." << std::endl;
            g_logger.log_event("ESC pressed in display window. Shutting down.");
            break;
        }

#ifdef _WIN32
        if (_kbhit()) {
            const int console_key = _getch();
            if (console_key == 27 || console_key == 'q' || console_key == 'Q') {
                std::cout << "\n[MAIN] Console quit key pressed. Shutting down." << std::endl;
                g_logger.log_event("Console quit key pressed. Shutting down.");
                break;
            }
        }

        if (g_shutdown_requested.load()) {
            std::cout << "\n[MAIN] Console control event received. Shutting down." << std::endl;
            g_logger.log_event("Console control event received. Shutting down.");
            break;
        }
#endif
    }

    g_logger.log_event("Requesting pipeline shutdown (GST_STATE_NULL)");
    const GstStateChangeReturn shutdown_ret = gst_element_set_state(pipeline, GST_STATE_NULL);
    g_logger.log_event(std::string("Shutdown state change return: ") + gst_element_state_change_return_get_name(shutdown_ret));

    GstState current_state = GST_STATE_VOID_PENDING;
    GstState pending_state = GST_STATE_VOID_PENDING;
    const GstStateChangeReturn wait_ret = gst_element_get_state(pipeline, &current_state, &pending_state, 2 * GST_SECOND);
    std::ostringstream shutdown_text;
    shutdown_text << "Shutdown wait result: " << gst_element_state_change_return_get_name(wait_ret)
                  << " | current=" << gst_element_state_get_name(current_state)
                  << " | pending=" << gst_element_state_get_name(pending_state);
    g_logger.log_event(shutdown_text.str());

    gst_object_unref(pipeline);
    cv::destroyAllWindows();
    g_logger.log_event("Session stopped");
    std::cout << "[INFO] Done." << std::endl;
    return 0;
}

