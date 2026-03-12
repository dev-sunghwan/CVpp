#include <iostream>
#include <string>
#include <mutex>
#include <map>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/rtsp/rtsp.h>
#include <opencv2/opencv.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

// 스레드 간 프레임 공유를 위한 뮤텍스와 버퍼
std::mutex frame_mutex;
cv::Mat current_frame;
bool new_frame_available = false;

// =========================================================================
// GStreamer rtspsrc before-send Callback
// =========================================================================
static gboolean on_before_send(GstElement* /* rtspsrc */, GstRTSPMessage* message, gpointer user_data) {
    if (!user_data) return TRUE;
    
    // Cast user_data back to our std::map containing custom headers
    auto* custom_headers = static_cast<std::map<std::string, std::string>*>(user_data);

    if (message->type == GST_RTSP_MESSAGE_REQUEST) {
        GstRTSPMethod method;
        const gchar* uri;
        
        if (gst_rtsp_message_parse_request(message, &method, &uri, NULL) == GST_RTSP_OK) {
            // Depending on the camera, some headers are needed in DESCRIBE/SETUP as well.
            // For now, we inject into the PLAY request.
            if (method == GST_RTSP_PLAY) {
                std::cout << "\n==================================================" << std::endl;
                std::cout << "[GSTREAMER] Intercepted PLAY request!" << std::endl;
                std::cout << "[GSTREAMER] Injecting custom headers..." << std::endl;
                std::cout << "==================================================\n" << std::endl;
                
                for (const auto& kv : *custom_headers) {
                    gst_rtsp_message_add_header_by_name(message, kv.first.c_str(), kv.second.c_str());
                    std::cout << " -> Injected: " << kv.first << ": " << kv.second << std::endl;
                }
            }
        }
    }
    return TRUE;
}

// =========================================================================
// GStreamer AppSink Callback
// =========================================================================
// This thread is called every time a new decoded frame arrives in the pipeline.
static GstFlowReturn on_new_sample(GstElement* sink, gpointer user_data) {
    GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
    if (!sample) return GST_FLOW_ERROR;

    // Extract buffer and metadata (caps)
    GstBuffer* buffer = gst_sample_get_buffer(sample);
    GstCaps* caps = gst_sample_get_caps(sample);
    GstStructure* structure = gst_caps_get_structure(caps, 0);

    int width, height;
    gst_structure_get_int(structure, "width", &width);
    gst_structure_get_int(structure, "height", &height);

    // Access pixel data (Zero-copy mapping)
    GstMapInfo map;
    gst_buffer_map(buffer, &map, GST_MAP_READ);

    // Wrap GStreamer's BGR data into OpenCV's cv::Mat
    cv::Mat frame(height, width, CV_8UC3, (char*)map.data, cv::Mat::AUTO_STEP);

    // Thread-safely copy the frame to the main thread's display queue
    {
        std::lock_guard<std::mutex> lock(frame_mutex);
        frame.copyTo(current_frame); // Deep copy for display
        new_frame_available = true;
    }

    // Release memory
    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);

    return GST_FLOW_OK;
}

int main(int argc, char *argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    std::cout << "==================================================" << std::endl;
    std::cout << "[MAIN] GStreamer Video Analytics Pipeline" << std::endl;
    std::cout << "==================================================" << std::endl;

    // Define custom headers to inject here
    std::map<std::string, std::string> custom_headers = {
        {"Rate-Control", "no"}
        // You can add more headers here easily, e.g.:
        // {"Scale", "1.0"},
        // {"Blocksize", "65536"}
    };

    // 1. Initialize GStreamer core
    gst_init(&argc, &argv);
    std::cout << "[INFO] GStreamer core engine initialized." << std::endl;

    // 2. Build Pipeline string 
    // decodebin automatically handles H.264/RTP hardware or software decoding.
    std::string rtsp_url = "rtsp://admin:Sunap1!!@192.168.4.225/profile10/media.smp";
    
    std::string pipeline_str = 
        "rtspsrc name=mysrc location=" + rtsp_url + " latency=100 ! "
        "decodebin ! videoconvert ! video/x-raw,format=BGR ! "
        "appsink name=mysink emit-signals=true max-buffers=2 drop=true sync=false";

    GError* error = nullptr;
    GstElement* pipeline = gst_parse_launch(pipeline_str.c_str(), &error);
    
    if (error) {
        std::cerr << "[ERROR] Failed to create GStreamer pipeline: " << error->message << std::endl;
        g_clear_error(&error);
        return 1;
    }

    std::cout << "[INFO] GStreamer pipeline elements assembled successfully." << std::endl;

    // 3. Connect callbacks to rtspsrc and appsink
    GstElement* rtspsrc = gst_bin_get_by_name(GST_BIN(pipeline), "mysrc");
    if (rtspsrc) {
        // Pass the address of custom_headers map as user_data
        g_signal_connect(rtspsrc, "before-send", G_CALLBACK(on_before_send), &custom_headers);
        gst_object_unref(rtspsrc);
    } else {
        std::cerr << "[WARN] rtspsrc element not found; cannot inject custom headers." << std::endl;
    }

    GstElement* appsink = gst_bin_get_by_name(GST_BIN(pipeline), "mysink");
    if (!appsink) {
        std::cerr << "[ERROR] appsink element not found." << std::endl;
        return 1;
    }

    // Connect appsink to on_new_sample trigger
    g_signal_connect(appsink, "new-sample", G_CALLBACK(on_new_sample), nullptr);
    gst_object_unref(appsink);

    // 4. Start Pipeline (Transition to PLAYING state)
    // At this moment, background threads begin RTSP communication and decoding.
    std::cout << "[INFO] Requesting RTSP stream setup and decoding start..." << std::endl;
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "[ERROR] Failed to change pipeline state to PLAYING." << std::endl;
        gst_object_unref(pipeline);
        return 1;
    }

    std::cout << "[INFO] Streaming successfully. Press ESC in the display window to exit." << std::endl;

    // 5. Main thread display loop
    while (true) {
        cv::Mat display_frame;
        {
            std::lock_guard<std::mutex> lock(frame_mutex);
            if (new_frame_available) {
                display_frame = current_frame.clone();
                new_frame_available = false;
            }
        }

        if (!display_frame.empty()) {
            cv::imshow("GStreamer + OpenCV Pipeline", display_frame);
        }

        if (cv::waitKey(30) == 27) { // 27 = ESC key
            std::cout << "\n[MAIN] ESC key pressed. Initiating shutdown procedure." << std::endl;
            break;
        }
    }

    // 6. Cleanup Resources
    std::cout << "[INFO] Stopping pipeline and releasing memory resources..." << std::endl;
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    cv::destroyAllWindows();
    
    std::cout << "[INFO] Program exited normally." << std::endl;
    return 0;
}
