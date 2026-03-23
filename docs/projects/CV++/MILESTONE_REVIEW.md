# CV++ Milestone Review

## Document Control
- Version: `v0.1`
- Status: `Drafted`
- Created: `2026-03-23`
- Last Updated: `2026-03-23`
- Owner: `PM / Tech Lead / Software Engineer Agents`

## Summary
The current milestone should be treated as a successful foundation milestone. The team clarified the real product goal, corrected earlier false conclusions about profile-specific metadata behavior, stabilized the RTSP control path enough for continued development, and restored multi-object overlay behavior in the main app.

## What Was Achieved
- externalized RTSP configuration into `config.toml`
- added session-based logging under `output/session-.../`
- captured raw metadata and parsed summaries for each session
- confirmed graceful RTSP shutdown with `PAUSE` and `TEARDOWN`
- created `metadata_probe` as a control experiment for camera behavior
- corrected the earlier wrong conclusion that `profile2` and `profile4` had different metadata semantics
- aligned the full app metadata session with the control probe by consuming selected auxiliary video pads
- restored multi-object parsing and multi-object overlay in live runs

## Key Findings
- the camera profiles themselves were not the main issue
- stale `gst-launch` sessions and incomplete probe paths produced misleading conclusions
- live metadata often arrives fragmented, so parser and session logic must tolerate partial payloads
- the main app can now receive and render multiple simultaneous objects
- object counts seen in logs are detection events, not unique object counts

## Current Verified State
The app can now:
- receive RTSP video from the Hanwha camera
- receive ONVIF/WiseAI metadata
- log raw metadata and parsed summaries
- render overlays for multiple simultaneous objects
- show that `Car`, `Human`, and `Bicycle` are all present in real sessions

## Remaining Gaps
- evidence visibility is still too implicit; the user should not need to inspect logs to know whether metadata arrived
- metadata performance is still measured mostly as repeated detections, not camera-reported unique objects
- fragmented payloads can still distort type labels and per-frame object completeness

## Decision
Close the current milestone as a foundation and observability milestone.

Open the next milestone with a tighter product goal: make metadata evidence and metadata performance explicit in the runtime experience.

## Next Milestone Goal
The next milestone should answer two user questions directly:
1. Did the camera actually send metadata for the object I am looking at?
2. How good is the camera's metadata performance over a session?

That means the next milestone should add:
- an evidence banner or evidence panel showing raw-seen, parsed-count, overlay-count, and metadata age
- session metrics for detections by type and unique object IDs by type
- a clear distinction between repeated detection events and unique tracked objects
- lightweight summary output that helps SungHwan evaluate camera metadata quality without manual log digging

## Recommendation
Do not pivot away from the current architecture. Continue from the current app and control probe baseline, and focus the next milestone on evidence visibility and metadata performance reporting.
