# CV++

CV++ is a C++-based RTSP verification tool for Hanwha Vision camera and NVR environments. The current MVP focus is not external AI yet. It is direct RTSP control, custom header injection, ONVIF-style metadata capture, and operator-friendly verification of raw versus parsed metadata.

## Current Direction
The approved v0.1 direction is:
- stable RTSP streaming in C++
- configurable custom RTSP headers
- raw metadata capture and logging
- parsed metadata visibility
- overlay validation with freshness handling
- a minimal verification-oriented UI

The project is being developed as a practical metadata-observability product first. AI comparison features remain out of scope until metadata capture and trustworthiness are solid.

## Current Status
Milestone 1 is complete.

Completed so far:
- GStreamer-based RTSP pipeline
- `before-send` custom header injection
- video and metadata appsink routing
- metadata parsing and overlay prototype
- TOML-based runtime configuration
- session-based output folders
- plain-file raw metadata logging

Current limitations:
- most responsibilities still sit in `main.cpp`
- parser behavior is still regex-based
- raw and parsed comparison is not yet summarized clearly in the UI
- reconnect behavior is not implemented yet

## Repository Layout
- `main.cpp`: current prototype entry point
- `config.toml`: runtime configuration for RTSP and logging
- `app_config.*`: config loading
- `session_logger.*`: session and raw metadata logging
- `metadata_types.h`: shared detection structure
- `docs/team/TEAM_CHARTER.md`: Team SH operating rules
- `docs/projects/CV++/`: project decisions, architecture, and tasks

## Build
Requirements:
- Windows 11
- CMake
- Visual Studio C++ Build Tools
- GStreamer 1.28.x MSVC 64-bit
- OpenCV build referenced from `CMakeLists.txt`

Build commands:
```powershell
cmake -S . -B build
cmake --build build --config Release
```

## Run
1. Update `config.toml` with the real RTSP URL and credentials.
2. Ensure GStreamer and OpenCV runtime DLL paths are available.
3. Run the executable:

```powershell
$env:PATH += ";C:\Program Files\gstreamer\1.0\msvc_x86_64\bin"
.\build\Release\RTSP_Viewer.exe
```

Each run creates an `output/session-YYYYMMDD-HHMMSS/` folder with:
- `session.log`
- `metadata_raw.xml.log`

## Project Docs
Use these as the current source of truth:
- `docs/team/TEAM_CHARTER.md`
- `docs/projects/CV++/DECISIONS.md`
- `docs/projects/CV++/ARCHITECTURE.md`
- `docs/projects/CV++/TASKS.md`
