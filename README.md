# CV++

CV++ is a C++-based RTSP verification tool for Hanwha Vision camera and NVR environments.

I started the project for two reasons at once: to build a practical metadata-observability tool around a real video workflow, and to learn modern C++ through a project with concrete operational constraints instead of isolated exercises.

## Why this project exists

The current focus is not external AI comparison yet. The first problem to solve is trustworthiness: when a camera emits ONVIF-style metadata, I want to verify what was actually sent, how it was parsed, and whether the overlay shown to an operator matches the underlying stream.

That makes CV++ both a useful work-adjacent prototype and a structured learning project in:

- C++ application design
- GStreamer-based RTSP handling
- metadata parsing and verification
- desktop UI work with Qt
- session logging and local persistence with SQLite

## Current direction

The approved v0.1 direction is:

- stable RTSP streaming in C++
- configurable custom RTSP headers
- raw metadata capture and logging
- parsed metadata visibility
- overlay validation with freshness handling
- a minimal verification-oriented UI

The project is being developed as a practical metadata-observability product first. AI comparison features remain out of scope until metadata capture and trustworthiness are solid.

## Current status

The project has moved beyond the initial bring-up stage. It already includes:

- GStreamer-based RTSP pipelines
- custom header injection
- metadata capture and parser-health reporting
- Qt verification shell work
- session logging and an initial SQLite review foundation

The main remaining work is to improve operator-facing review flows, keep the codebase modular as it grows, and continue replacing brittle pieces with more robust implementations.

## Repository layout

- `main.cpp` - current prototype entry point
- `config.toml` - runtime configuration for RTSP and logging
- `app_config.*` - config loading
- `session_logger.*` - session and raw metadata logging
- `metadata_types.h` - shared detection structure
- `docs/team/TEAM_CHARTER.md` - Team SH operating rules
- `docs/projects/CV++/` - project decisions, architecture, and tasks

## Build

Requirements:

- Windows 11
- CMake
- Visual Studio C++ Build Tools
- GStreamer 1.28.x MSVC 64-bit
- OpenCV build referenced from `CMakeLists.txt`

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

Each run creates an `output/session-YYYYMMDD-HHMMSS/` folder with raw and summarized session artifacts.

## Project docs

Use these as the current source of truth:

- `docs/team/TEAM_CHARTER.md`
- `docs/projects/CV++/DECISIONS.md`
- `docs/projects/CV++/ARCHITECTURE.md`
- `docs/projects/CV++/TASKS.md`
