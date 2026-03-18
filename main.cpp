#include <iostream>
#include <sstream>
#include <string>
#include <mutex>
#include <vector>
#include <regex>
#include <chrono>
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
    if (!user_data) {
        return TRUE;
    }
    auto* custom_headers = static_cast<std::map<std::string, std::string>*>(user_data);

    if (message->type == GST_RTSP_MESSAGE_REQUEST) {
        GstRTSPMethod method;
        const gchar* uri;
        if (gst_rtsp_message_parse_request(message, &method, &uri, NULL) == GST_RTSP_OK) {
            if (method == GST_RTSP_DESCRIBE || method == GST_RTSP_PLAY || method == GST_RTSP_SETUP) {
                for (const auto& kv : *custom_headers) {
                    gst_rtsp_message_add_header_by_name(message, kv.first.c_str(), kv.second.c_str());
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
    GstStructure* str = gst_caps_get_structure(caps, 0);
    const gchar* name = gst_structure_get_name(str);

    if (g_str_has_prefix(name, "video/x-raw")) {
        if (gst_pad_link(new_pad, sink_pad) == GST_PAD_LINK_OK) {
            std::cout << "[INFO] decodebin successfully linked to videoconvert." << std::endl;
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

    GstStructure* structure = gst_caps_get_structure(caps, 0);
    const gchar* media = gst_structure_get_string(structure, "media");
    std::string media_str = media ? media : "unknown";
    const gchar* encoding_name = gst_structure_get_string(structure, "encoding-name");
    std::string encoding_str = encoding_name ? encoding_name : "unknown";

    std::cout << "[INFO] rtspsrc added a dynamic pad: " << media_str << " / " << encoding_str << std::endl;

    if (media_str == "video" && encoding_str == "H264") {
        GstElement* decodebin = gst_bin_get_by_name(GST_BIN(pipeline), "decodebin");
        GstPad* sink_pad = gst_element_get_static_pad(decodebin, "sink");
        if (!gst_pad_is_linked(sink_pad)) {
            if (gst_pad_link(new_pad, sink_pad) == GST_PAD_LINK_OK) {
                std::cout << "[INFO] Linked H264 video pad to decodebin." << std::endl;
            }
        } else {
            std::cout << "[INFO] decodebin sink already linked, ignoring extra pad." << std::endl;
        }
        gst_object_unref(sink_pad);
        gst_object_unref(decodebin);
    } else if (media_str == "application" &&
               (encoding_str == "vnd.onvif.metadata" || encoding_str == "VND.ONVIF.METADATA")) {
        GstElement* meta_sink = gst_bin_get_by_name(GST_BIN(pipeline), "meta_sink");
        GstPad* sink_pad = gst_element_get_static_pad(meta_sink, "sink");
        if (gst_pad_link(new_pad, sink_pad) == GST_PAD_LINK_OK) {
            std::cout << "[INFO] Linked ONVIF metadata pad to meta_sink." << std::endl;
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
    cv::Mat frame(height, width, CV_8UC3, (char*)map.data, cv::Mat::AUTO_STEP);

    {
        std::lock_guard<std::mutex> lock(frame_mutex);
        frame.copyTo(current_frame);
        new_frame_available = true;
    }

    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);
    return GST_FLOW_OK;
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
    g_logger.log_event("Config loaded from config.toml");
    g_logger.log_event(std::string("Fixture candidate capture: ") + (config.capture_fixture_candidates ? "enabled" : "disabled"));

    gst_init(&argc, &argv);
    std::cout << "[INFO] GStreamer core engine initialized." << std::endl;

    GstElement* pipeline = gst_pipeline_new("analytics_pipeline");
    GstElement* rtspsrc = gst_element_factory_make("rtspsrc", "mysrc");
    GstElement* decodebin = gst_element_factory_make("decodebin", "decodebin");
    GstElement* vconv = gst_element_factory_make("videoconvert", "vconv");
    GstElement* video_sink = gst_element_factory_make("appsink", "video_sink");
    GstElement* meta_sink = gst_element_factory_make("appsink", "meta_sink");

    if (!pipeline || !rtspsrc || !decodebin || !vconv || !video_sink || !meta_sink) {
        std::cerr << "[ERROR] Failed to create GStreamer elements." << std::endl;
        g_logger.log_event("Failed to create GStreamer elements");
        return 1;
    }

    g_object_set(G_OBJECT(rtspsrc), "location", config.rtsp_url.c_str(), "latency", config.latency, NULL);
    g_signal_connect(rtspsrc, "before-send", G_CALLBACK(on_before_send), &config.headers);

    GstCaps* caps_v = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "BGR", NULL);
    g_object_set(G_OBJECT(video_sink), "caps", caps_v, "emit-signals", TRUE, "sync", FALSE,
                 "max-buffers", 2, "drop", TRUE, NULL);
    gst_caps_unref(caps_v);

    g_object_set(G_OBJECT(meta_sink), "emit-signals", TRUE, "sync", FALSE, NULL);

    gst_bin_add_many(GST_BIN(pipeline), rtspsrc, decodebin, vconv, video_sink, meta_sink, NULL);

    if (!gst_element_link(vconv, video_sink)) {
        std::cerr << "[ERROR] Failed to link vconv to video_sink." << std::endl;
        g_logger.log_event("Failed to link vconv to video_sink");
        return 1;
    }

    g_signal_connect(rtspsrc, "pad-added", G_CALLBACK(on_src_pad_added), pipeline);
    g_signal_connect(decodebin, "pad-added", G_CALLBACK(on_decodebin_pad_added), vconv);
    g_signal_connect(video_sink, "new-sample", G_CALLBACK(on_new_video_sample), nullptr);
    g_signal_connect(meta_sink, "new-sample", G_CALLBACK(on_new_meta_sample), &config);

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

    while (true) {
        GstBus* bus = gst_element_get_bus(pipeline);
        GstMessage* msg = gst_bus_pop(bus);
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
                    if (err) {
                        g_error_free(err);
                    }
                    if (dbg) {
                        g_free(dbg);
                    }
                    break;
                default:
                    break;
            }
            gst_message_unref(msg);
        }
        gst_object_unref(bus);

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
                const auto now = std::chrono::steady_clock::now();
                const bool metadata_is_fresh =
                    has_metadata_update &&
                    (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_metadata_update).count() <= 500);

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

        if (cv::waitKey(30) == 27) {
            std::cout << "\n[MAIN] ESC pressed. Shutting down." << std::endl;
            g_logger.log_event("ESC pressed. Shutting down.");
            break;
        }
    }

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    cv::destroyAllWindows();
    g_logger.log_event("Session stopped");
    std::cout << "[INFO] Done." << std::endl;
    return 0;
}
