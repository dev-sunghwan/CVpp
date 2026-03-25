# PROJECT_BRIEF

## Document Control
- Version: `v0.3`
- Status: `Updated with thermal metadata future direction`
- Created: `2026-03-18`
- Last Updated: `2026-03-25`
- Owner: `Project Owner + Team SH`

## Change History
| Date | Version | Summary |
| --- | --- | --- |
| 2026-03-18 | v0.1 | Initial English brief imported into the repository. |
| 2026-03-18 | v0.2 | Updated project status and direction to match approved MVP scope and Milestone 1 progress. |
| 2026-03-25 | v0.3 | Added thermal camera metadata validation to the mid-term expansion direction. |

## 1. Project Overview
`CV++` is a C++-based RTSP streaming and metadata verification project for Hanwha Vision camera and NVR environments.

The current v0.1 goals are:
1. stream RTSP video reliably from Hanwha devices in C++
2. inject configurable custom RTSP headers
3. capture, parse, and verify ONVIF-style metadata against overlays and raw evidence

## 2. Why This Project Exists
The earlier Python/OpenCV approach was useful for fast experiments, but it did not provide enough control for RTSP request customization and metadata observability.

The project moved to GStreamer so that `rtspsrc` and its `before-send` callback could control outgoing RTSP requests directly. That makes it possible to validate custom headers and build a more trustworthy metadata pipeline.

## 3. Current Implementation Status
Already working or present in prototype form:
- GStreamer-based RTSP pipeline
- custom RTSP header injection
- split handling of video and ONVIF metadata streams
- metadata parsing for detected objects
- overlay rendering with freshness-based clearing
- TOML-based config loading
- session-based output folders
- plain-file raw metadata logging

Current pipeline concept:
```text
rtspsrc -> decodebin -> videoconvert -> appsink
        -> metadata appsink
```

## 4. Current Product Direction
The project should be treated as a metadata-observability tool first, not as a general AI video analytics platform.

Near-term priority:
- verify what metadata the camera is actually sending
- compare raw metadata and parsed output
- improve trust in overlay behavior
- prepare for a minimal one-screen verification workflow

## 5. Known Limitations
- much of the runtime flow is still concentrated in `main.cpp`
- parser behavior is still regex-based and needs better transparency
- parsed summary and raw evidence are not yet presented together clearly
- reconnect behavior is not implemented yet
- automated regression coverage is still limited

## 6. Near-Term Direction
The next milestone is metadata capture and parse transparency.

That means:
1. preserve raw metadata before parsing
2. surface parse failures and unknown patterns explicitly
3. collect a small set of real Hanwha metadata fixtures
4. make raw and parsed outputs comparable in the same session

## 7. Mid-Term Expansion
After metadata observability becomes reliable, the project can expand toward:
- comparing built-in camera AI with external CV models
- adding features not available on the camera itself
- evaluating outputs on a shared timeline

These remain out of scope for v0.1.

## 8. One-Line Summary
CV++ is building a practical C++ RTSP and metadata verification foundation for Hanwha environments, and the current focus is observability, not AI expansion.

