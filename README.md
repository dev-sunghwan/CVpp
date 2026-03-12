# CV++: RTSP Video Analytics Pipeline in C++

## Overview
This project is an educational and practical journey transitioning from Python (`cv2.VideoCapture`) to a highly optimized, custom C++ Video Analytics Pipeline. 

The primary goal is to build a robust streaming client that directly connects to a Hanwha Vision camera via RTSP, bypasses OpenCV's built-in network limitations, and allows for deep customization such as injecting arbitrary RTSP HTTP headers (`Rate-Control`, `Scale`, etc.) and processing frames with zero-copy mechanisms.

## Project History & Architectural Evolution

### Phase 1-4: The Python to C++ Transition
* **Goal**: Replicate the simplicity of Python's OpenCV RTSP capture in C++.
* **Action**: Set up the Visual Studio C++ environment, CMake, and linked OpenCV pre-built binaries for Windows. 
* **Result**: Successfully compiled a basic `cv::VideoCapture` loop in C++. Gained understanding of C++ compilation, linking, and basic memory management (`cv::Mat` vs NumPy arrays).

### Phase 5: The "Custom Header" Challenge & Local Proxy Attempt
* **Problem**: The camera required specific custom headers (like `Rate-Control: no`) to stabilize the stream. OpenCV's FFmpeg backend completely strips or ignores custom headers during the RTSP `PLAY` phase.
* **Attempt 1 (Raw Sockets)**: Built a raw C++ TCP socket client from scratch to handle the RTSP handshake (OPTIONS, DESCRIBE, SETUP, PLAY) and Digest Authentication (using Windows Cryptography API for MD5).
* **Attempt 2 (Local Proxy)**: Designed a C++ multithreaded Local RTSP Proxy (`127.0.0.1:8554`). The idea was to intercept OpenCV's request, inject the header, and relay the RTP binary stream back to OpenCV for decoding.
* **Conclusion**: While educational, manually parsing arbitrary RTP packets and managing a transparent TCP proxy is error-prone and reinvents the wheel. A standard framework was needed.

### Phase 6-7: The GStreamer Paradigm Shift (Current Architecture)
* **Goal**: Achieve native custom header injection and Hardware-accelerated H.264 decoding without building a proxy.
* **Solution**: Integrated **GStreamer 1.28 (MSVC 64-bit)** as the core pipeline engine.
* **Implementation**:
  * Scrapped the raw socket code and replaced it with GStreamer's C++ API.
  * Pipeline: `rtspsrc ! decodebin ! videoconvert ! appsink`
  * **The Magic Hook**: Utilized `rtspsrc`'s `before-send` callback signal. Just before the `PLAY` request is dispatched over the network, the C++ code intercepts the `GstRTSPMessage` and dynamically injects multiple custom headers using a `std::map`.
  * **Memory Management**: The decoded BGR frames arrive asynchronously at the `appsink`. Using `std::mutex`, the frames are thread-safely deep-copied into the main thread's `cv::Mat` for display.
* **Result**: A highly performant, customizable, and industry-standard Video Analytics Pipeline foundation.

## Current Developer Environment
* **OS**: Windows 11
* **Language**: C++17
* **Build System**: CMake
* **Libraries**: OpenCV (Video Rendering), GStreamer 1.28.1 (RTSP Networking & Decoding)

## How to Build and Run
1. Install GStreamer 1.28.1 (MSVC 64-bit) Complete/Custom installation (ensure Development files are checked).
2. Install CMake and Visual Studio C++ Build Tools.
3. Build the project using CMake:
   ```bash
   cmake -B build
   cmake --build build --config Release
   ```
4. Run the executable (ensure GStreamer `bin` is in the `PATH`):
   ```powershell
   $env:PATH += ";C:\Program Files\gstreamer\1.0\msvc_x86_64\bin"
   .\build\Release\RTSP_Viewer.exe
   ```

## Next Steps
With the core RTSP and decoding pipeline secured, future milestones include:
1. Attaching AI/Deep Learning models (TensorRT / YOLO) for object detection.
2. Implementing Motion Detection and Event Logging.
3. Building an MQTT/REST API communication layer to broadcast events to external dashboards.
