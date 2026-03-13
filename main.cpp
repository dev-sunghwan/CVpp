#include <iostream>
#include <string>
#include <mutex>
#include <map>
#include <vector>
#include <regex>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/rtsp/rtsp.h>
#include <opencv2/opencv.hpp>

#ifdef _WIN32
#define NOMINMAX  // Prevent Windows.h from defining min/max macros
#include <windows.h>
#endif

// =========================================================================
// Shared Data Structures (Thread-safe between Meta and Display threads)
// =========================================================================
struct DetectedObject {
    int id = 0;
    std::string type = "Unknown";
    float likelihood = 0.0f;
    // Pixel-space bounding box from camera's absolute coords (width=1920/height=1080 assumed)
    float left = 0, top = 0, right = 0, bottom = 0;
};

cv::Mat current_frame;
std::vector<DetectedObject> current_objects;
std::mutex frame_mutex;
std::mutex meta_mutex;
bool new_frame_available = false;

// =========================================================================
// XML Metadata Parser
// =========================================================================
static std::vector<DetectedObject> parse_onvif_xml(const std::string& xml) {
    std::vector<DetectedObject> objects;

    // MSVC std::regex does not support dotall. We extract per-object blocks manually.
    // Split by <tt:Object and find each closing </tt:Object>
    const std::string start_tag = "<tt:Object ObjectId=";
    const std::string end_tag   = "</tt:Object>";

    std::regex id_re   ("ObjectId=\"(\\d+)\"");
    std::regex bbox_re ("<tt:BoundingBox left=\"([0-9.-]+)\" top=\"([0-9.-]+)\" right=\"([0-9.-]+)\" bottom=\"([0-9.-]+)\"");
    std::regex type_re ("<tt:Type Likelihood=\"([0-9.-]+)\">([^<]+)</tt:Type>");

    size_t pos = 0;
    while ((pos = xml.find(start_tag, pos)) != std::string::npos) {
        size_t end = xml.find(end_tag, pos);
        if (end == std::string::npos) break;
        end += end_tag.size();
        std::string block = xml.substr(pos, end - pos);
        pos = end;

        DetectedObject obj;
        std::smatch id_m;
        if (std::regex_search(block, id_m, id_re))
            obj.id = std::stoi(id_m[1].str());

        std::smatch bbox_m;
        if (std::regex_search(block, bbox_m, bbox_re)) {
            obj.left   = std::stof(bbox_m[1].str());
            obj.top    = std::stof(bbox_m[2].str());
            obj.right  = std::stof(bbox_m[3].str());
            obj.bottom = std::stof(bbox_m[4].str());
        }

        std::smatch type_m;
        if (std::regex_search(block, type_m, type_re)) {
            obj.likelihood = std::stof(type_m[1].str());
            obj.type       = type_m[2].str();
        }

        objects.push_back(obj);
        std::cout << "[META] ObjectId=" << obj.id
                  << " Type=" << obj.type
                  << " Likelihood=" << static_cast<int>(obj.likelihood * 100) << "%"
                  << " Box=[" << obj.left << "," << obj.top << "," << obj.right << "," << obj.bottom << "]"
                  << std::endl;
    }
    return objects;
}

// =========================================================================
// GStreamer rtspsrc before-send Callback
// =========================================================================
static gboolean on_before_send(GstElement*, GstRTSPMessage* message, gpointer user_data) {
    if (!user_data) return TRUE;
    auto* custom_headers = static_cast<std::map<std::string, std::string>*>(user_data);

    if (message->type == GST_RTSP_MESSAGE_REQUEST) {
        GstRTSPMethod method;
        const gchar* uri;
        if (gst_rtsp_message_parse_request(message, &method, &uri, NULL) == GST_RTSP_OK) {
            if (method == GST_RTSP_DESCRIBE || method == GST_RTSP_PLAY || method == GST_RTSP_SETUP) {
                for (const auto& kv : *custom_headers)
                    gst_rtsp_message_add_header_by_name(message, kv.first.c_str(), kv.second.c_str());
            }
        }
    }
    return TRUE;
}

// =========================================================================
// GStreamer pad-added Callbacks (Dynamic Routing)
// =========================================================================
static void on_decodebin_pad_added(GstElement*, GstPad* new_pad, gpointer user_data) {
    GstElement* vconv = GST_ELEMENT(user_data);
    GstPad* sink_pad  = gst_element_get_static_pad(vconv, "sink");
    if (gst_pad_is_linked(sink_pad)) { gst_object_unref(sink_pad); return; }

    GstCaps* caps = gst_pad_get_current_caps(new_pad);
    if (!caps) caps = gst_pad_query_caps(new_pad, NULL);
    GstStructure* str = gst_caps_get_structure(caps, 0);
    const gchar* name = gst_structure_get_name(str);

    if (g_str_has_prefix(name, "video/x-raw")) {
        if (gst_pad_link(new_pad, sink_pad) == GST_PAD_LINK_OK)
            std::cout << "[INFO] decodebin successfully linked to videoconvert." << std::endl;
    }
    gst_caps_unref(caps);
    gst_object_unref(sink_pad);
}

static void on_src_pad_added(GstElement*, GstPad* new_pad, gpointer user_data) {
    GstElement* pipeline = GST_ELEMENT(user_data);
    GstCaps* caps = gst_pad_get_current_caps(new_pad);
    if (!caps) caps = gst_pad_query_caps(new_pad, NULL);
    
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
            if (gst_pad_link(new_pad, sink_pad) == GST_PAD_LINK_OK)
                std::cout << "[INFO] Linked H264 video pad to decodebin." << std::endl;
        } else {
            std::cout << "[INFO] decodebin sink already linked, ignoring extra pad." << std::endl;
        }
        gst_object_unref(sink_pad);
        gst_object_unref(decodebin);
    } else if (media_str == "application" &&
               (encoding_str == "vnd.onvif.metadata" || encoding_str == "VND.ONVIF.METADATA")) {
        GstElement* meta_sink = gst_bin_get_by_name(GST_BIN(pipeline), "meta_sink");
        GstPad* sink_pad = gst_element_get_static_pad(meta_sink, "sink");
        if (gst_pad_link(new_pad, sink_pad) == GST_PAD_LINK_OK)
            std::cout << "[INFO] Linked ONVIF metadata pad to meta_sink." << std::endl;
        gst_object_unref(sink_pad);
        gst_object_unref(meta_sink);
    } else {
        std::cout << "[INFO] Ignoring pad: " << media_str << "/" << encoding_str << std::endl;
    }

    gst_caps_unref(caps);
}

// =========================================================================
// GStreamer AppSink Callbacks
// =========================================================================
static GstFlowReturn on_new_meta_sample(GstElement* sink, gpointer) {
    GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
    if (!sample) return GST_FLOW_ERROR;

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    GstMapInfo map;
    gst_buffer_map(buffer, &map, GST_MAP_READ);

    // Find start of XML (skip binary RTP headers before <?xml)
    const char* xml_start = nullptr;
    for (gsize i = 0; i + 4 < map.size; ++i) {
        if (map.data[i] == '<' && map.data[i+1] == '?' && map.data[i+2] == 'x') {
            xml_start = reinterpret_cast<const char*>(map.data + i);
            break;
        }
    }

    if (xml_start) {
        std::string xml_str(xml_start, reinterpret_cast<const char*>(map.data + map.size));
        auto objects = parse_onvif_xml(xml_str);
        if (!objects.empty()) {
            std::lock_guard<std::mutex> lock(meta_mutex);
            current_objects = std::move(objects);
        }
    }

    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

static GstFlowReturn on_new_video_sample(GstElement* sink, gpointer) {
    GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
    if (!sample) return GST_FLOW_ERROR;

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    GstCaps* caps = gst_sample_get_caps(sample);
    GstStructure* structure = gst_caps_get_structure(caps, 0);

    int width = 0, height = 0;
    gst_structure_get_int(structure, "width",  &width);
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

// =========================================================================
// Draw Overlay: Bounding Box + Label
// =========================================================================
static void draw_overlay(cv::Mat& frame, const std::vector<DetectedObject>& objects) {
    int fw = frame.cols;
    int fh = frame.rows;
    
    for (const auto& obj : objects) {
        // Camera sends absolute pixel coordinates (no normalization needed)
        int x1 = static_cast<int>(obj.left);
        int y1 = static_cast<int>(obj.top);
        int x2 = static_cast<int>(obj.right);
        int y2 = static_cast<int>(obj.bottom);

        // Clamp to frame bounds (manual for MSVC compatibility)
        auto clampI = [](int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); };
        x1 = clampI(x1, 0, fw - 1);
        y1 = clampI(y1, 0, fh - 1);
        x2 = clampI(x2, 0, fw - 1);
        y2 = clampI(y2, 0, fh - 1);

        // Draw bounding box
        cv::rectangle(frame, cv::Point(x1, y1), cv::Point(x2, y2),
                      cv::Scalar(0, 255, 0), 2);

        // Draw label background
        std::string label = obj.type + " " + std::to_string((int)(obj.likelihood * 100)) + "%";
        int baseline = 0;
        cv::Size text_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);
        int label_y = (y1 - 5 > text_size.height + 5) ? (y1 - 5) : (text_size.height + 5);
        
        cv::rectangle(frame,
                      cv::Point(x1, label_y - text_size.height - baseline - 4),
                      cv::Point(x1 + text_size.width, label_y + baseline),
                      cv::Scalar(0, 200, 0), cv::FILLED);
        cv::putText(frame, label, cv::Point(x1, label_y - 2),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0), 1);
    }
}

// =========================================================================
// Main
// =========================================================================
int main(int argc, char *argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    std::cout << "==================================================" << std::endl;
    std::cout << "[MAIN] GStreamer Video & Metadata Analytics" << std::endl;
    std::cout << "==================================================" << std::endl;

    std::map<std::string, std::string> custom_headers = {
        {"Rate-Control", "no"},
        {"Require", "Bestshot"}
    };

    gst_init(&argc, &argv);
    std::cout << "[INFO] GStreamer core engine initialized." << std::endl;

    std::string rtsp_url = "rtsp://admin:Sunap1!!@192.168.4.225/profile10/media.smp";

    GstElement* pipeline  = gst_pipeline_new("analytics_pipeline");
    GstElement* rtspsrc   = gst_element_factory_make("rtspsrc",      "mysrc");
    GstElement* decodebin = gst_element_factory_make("decodebin",     "decodebin");
    GstElement* vconv     = gst_element_factory_make("videoconvert",  "vconv");
    GstElement* video_sink= gst_element_factory_make("appsink",       "video_sink");
    GstElement* meta_sink = gst_element_factory_make("appsink",       "meta_sink");

    if (!pipeline || !rtspsrc || !decodebin || !vconv || !video_sink || !meta_sink) {
        std::cerr << "[ERROR] Failed to create GStreamer elements." << std::endl;
        return 1;
    }

    g_object_set(G_OBJECT(rtspsrc), "location", rtsp_url.c_str(), "latency", 100, NULL);
    g_signal_connect(rtspsrc, "before-send", G_CALLBACK(on_before_send), &custom_headers);

    GstCaps* caps_v = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "BGR", NULL);
    g_object_set(G_OBJECT(video_sink), "caps", caps_v, "emit-signals", TRUE, "sync", FALSE,
                 "max-buffers", 2, "drop", TRUE, NULL);
    gst_caps_unref(caps_v);

    g_object_set(G_OBJECT(meta_sink), "emit-signals", TRUE, "sync", FALSE, NULL);

    gst_bin_add_many(GST_BIN(pipeline), rtspsrc, decodebin, vconv, video_sink, meta_sink, NULL);

    if (!gst_element_link(vconv, video_sink)) {
        std::cerr << "[ERROR] Failed to link vconv to video_sink." << std::endl;
        return 1;
    }

    g_signal_connect(rtspsrc,   "pad-added", G_CALLBACK(on_src_pad_added),      pipeline);
    g_signal_connect(decodebin, "pad-added", G_CALLBACK(on_decodebin_pad_added), vconv);
    g_signal_connect(video_sink, "new-sample", G_CALLBACK(on_new_video_sample),  nullptr);
    g_signal_connect(meta_sink,  "new-sample", G_CALLBACK(on_new_meta_sample),   nullptr);

    std::cout << "[INFO] Starting pipeline..." << std::endl;
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "[ERROR] Failed to change pipeline state to PLAYING." << std::endl;
        gst_object_unref(pipeline);
        return 1;
    }
    std::cout << "[INFO] Streaming. Press ESC to exit." << std::endl;

    while (true) {
        // Poll GStreamer Bus for errors
        GstBus* bus = gst_element_get_bus(pipeline);
        GstMessage* msg = gst_bus_pop(bus);
        if (msg) {
            GError* err = nullptr; gchar* dbg = nullptr;
            switch (GST_MESSAGE_TYPE(msg)) {
                case GST_MESSAGE_ERROR:
                    gst_message_parse_error(msg, &err, &dbg);
                    std::cerr << "[GST ERROR] " << (err ? err->message : "?") << std::endl;
                    if (dbg) std::cerr << "[GST DEBUG] " << dbg << std::endl;
                    if (err) g_error_free(err);  if (dbg) g_free(dbg);
                    break;
                case GST_MESSAGE_WARNING:
                    gst_message_parse_warning(msg, &err, &dbg);
                    std::cerr << "[GST WARN]  " << (err ? err->message : "?") << std::endl;
                    if (err) g_error_free(err);  if (dbg) g_free(dbg);
                    break;
                default: break;
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
            // Fetch latest objects and draw overlay
            {
                std::lock_guard<std::mutex> lock(meta_mutex);
                draw_overlay(display_frame, current_objects);
            }
            cv::imshow("GStreamer Analytics (ESC to quit)", display_frame);
        }

        if (cv::waitKey(30) == 27) {
            std::cout << "\n[MAIN] ESC pressed. Shutting down." << std::endl;
            break;
        }
    }

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    cv::destroyAllWindows();
    std::cout << "[INFO] Done." << std::endl;
    return 0;
}
