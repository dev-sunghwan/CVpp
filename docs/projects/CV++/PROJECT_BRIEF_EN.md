# PROJECT_BRIEF

## 1. Project Overview

`CV++` is a C++-based RTSP streaming and video analytics project.
Its original purpose was to move beyond the simplicity of Python/OpenCV-based RTSP handling
and build a lower-level streaming pipeline that can be controlled more directly,
especially for Hanwha Vision camera and NVR environments.

The project is centered around three core goals:

1. Build a stable RTSP streaming application for IP cameras in C++
2. Inject custom RTSP headers required or supported by Hanwha cameras/NVRs
3. Parse RTSP metadata, especially ONVIF-style metadata, and validate it through on-screen overlays

## 2. Why This Project Exists

The earlier `cv::VideoCapture`-based approach was convenient for quick experiments,
but it had clear limitations when deeper RTSP control was needed,
especially for inserting custom headers during `SETUP` and `PLAY` requests.

To address that, the project was restructured around GStreamer.
The most important technical idea is the use of the `before-send` callback on `rtspsrc`,
which allows custom headers to be injected into RTSP messages immediately before they are sent over the network.

## 3. Current Implementation Status

The current implementation already covers the following:

- GStreamer-based RTSP pipeline
- Streaming from a Hanwha camera
- Custom RTSP header injection
- Separate handling of video and ONVIF metadata streams
- Metadata parsing for detected objects
- Bounding box overlay rendering

The current pipeline concept is roughly:

```text
rtspsrc -> decodebin -> videoconvert -> appsink
        -> metadata appsink
```

Video frames are pulled into OpenCV as `cv::Mat`,
while metadata is received separately, parsed from XML, and reflected in the overlay layer.

## 4. Recent Fixes

Recent work focused on metadata overlay issues, including:

- Fixing stale bounding boxes that remained visible after objects disappeared
- Adding metadata freshness handling so outdated metadata is no longer rendered
- Expanding parsing coverage to include patterns such as `VehicleInfo`, `HumanInfo`, `Class`, and `ClassCandidate`
- Adjusting label placement so text is less likely to be drawn outside the frame

## 5. Known Limitations

The project is currently closer to a working prototype than a fully structured product.
Known limitations include:

- RTSP URL and some options are still hardcoded
- Metadata parsing is still regex-based and may be fragile against structural variation
- There is no proper metadata monitoring or storage layer yet
- The code is still centered around a single `main.cpp`, which will become harder to extend
- Automated testing is limited, and several files are still experiment-oriented utilities

## 6. Near-Term Direction

The next practical priority is improving metadata observability and trustworthiness.

The most important near-term tasks are:

1. Add a way to capture and inspect raw XML metadata
2. Summarize parsed metadata in a format that is easy for humans to verify
3. Prepare for lightweight persistence, likely with SQLite, if metadata history becomes useful
4. Move RTSP URL, headers, and latency settings out of hardcoded values

In short, the next step is not adding AI first,
but making it possible to verify what metadata the camera is actually sending and how reliable it is.

## 7. Mid-Term Expansion

The long-term direction is to turn this from a streaming viewer into a comparison and extension platform
for built-in camera AI and external computer vision models.

Planned expansion areas include:

- Comparing Hanwha camera AI output with results from external CV models such as YOLO or SAM on the same RTSP stream
- Implementing features that are not currently available on the camera itself, such as face recognition
- Building a structure that can compare camera metadata and external model output on the same timeline

This makes metadata quality and observability a prerequisite for later model evaluation.

## 8. Recommended Next Milestones

1. Refactor `main.cpp` into clearer functional modules
2. Externalize configuration through a config file or simple UI input
3. Add raw metadata logging and parsed metadata monitoring
4. Decide whether SQLite-backed metadata storage is worth introducing
5. Define clean interfaces for integrating external CV models

## 9. One-Line Summary

This project is building a practical C++ foundation for controlling Hanwha RTSP streams directly,
including custom headers and metadata handling, and the next critical step is improving metadata observability and reliability.
