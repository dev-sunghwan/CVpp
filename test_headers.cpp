#include <iostream>
#include <vector>
#include <string>
#include <opencv2/opencv.hpp>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    // Set FFmpeg capture options via environment variable
    // We want to pass custom headers like "Rate-Control: no" and "Scale: 2.0"
    // The format is "key;value|key2;value2"
    // For FFmpeg rtsp, the option is "headers" and the value is the header string with \r\n
    // Let's try inserting multiple headers
    
    std::string env_opts = "headers;Rate-Control: no\r\nScale: 2.0\r\nRange: clock=20240101T000000Z-\r\n";
    _putenv_s("OPENCV_FFMPEG_CAPTURE_OPTIONS", env_opts.c_str());

    std::string source = "rtsp://admin:Sunap1!!@192.168.4.225/profile10/media.smp";

    std::cout << "[INFO] 연결: " << source << std::endl;
    cv::VideoCapture cap(source, cv::CAP_FFMPEG);

    if (!cap.isOpened()) {
        std::cerr << "[ERROR] 열 수 없습니다." << std::endl;
        return -1;
    }

    std::cout << "[INFO] 연결 성공" << std::endl;
    
    cv::Mat frame;
    cap >> frame;
    if (!frame.empty()) {
        std::cout << "[SUCCESS] 프레임을 성공적으로 가져왔습니다." << std::endl;
    }

    cap.release();
    return 0;
}
