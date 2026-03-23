# CV++ Verification Guide

## Purpose
Use CV++ as a camera metadata verification tool, not as an AI detector. The app shows what the camera sent, what the parser recovered, and what is currently rendered on screen.

## Start A Session
When the app starts, use the connection setup window to enter:
- camera IP address
- username
- password
- profile number

The runtime RTSP URL is built as:
- `rtsp://<user>:<password>@<ip>/profile<profile>/media.smp`

For normal testing, start with `profile2` unless you are intentionally comparing another profile.

## Screen Layout
- Left: live RTSP video with metadata overlay
- Right / Evidence: current metadata state for the frame you are looking at
- Right / Session Metrics: cumulative detections and unique camera-reported object IDs
- Right / Recent Metadata: recent parsed summaries for quick spot checks

## How To Read The Evidence Panel
- `raw=seen/not-seen`: whether raw metadata payloads have arrived
- `parsed=N`: how many objects the parser recovered from the latest payload
- `overlay=N`: how many objects are currently rendered on the video
- `age=...ms`: time since the last raw metadata payload
- `fresh=yes/no`: whether the current overlay is still inside the freshness window

## How To Interpret Missing Overlay
- `raw=not-seen`: the camera did not send usable metadata recently
- `raw=seen`, `parsed=0`: metadata arrived, but the latest payload did not contain usable objects
- `parsed>0`, `overlay=0`: metadata was parsed, but the overlay is stale or not currently active
- `parsed>0`, `overlay>0`: the camera sent object metadata and the app is showing it

## How To Read Session Metrics
- `Detections by type`: repeated detection events across the session
- `Unique object IDs`: unique camera-reported `ObjectId` values seen in the session

Use both numbers together:
- high detections + low unique IDs usually means the same object stayed in view for a long time
- low detections + low unique IDs may mean the camera rarely emitted object metadata

## Recommended Verification Workflow
1. Confirm the video stream is stable.
2. Watch the Evidence panel while an object is in the FoV.
3. If overlay is missing, check whether `raw` and `parsed` changed.
4. Use Recent Metadata to confirm object type and object count.
5. At session end, review `session.log`, `parsed_summary.log`, and Session Metrics together.
