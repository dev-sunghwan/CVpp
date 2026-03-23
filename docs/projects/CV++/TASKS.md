# CV++ Tasks

## Document Control
- Version: `v0.7`
- Status: `Milestone 3 ready`
- Created: `2026-03-18`
- Last Updated: `2026-03-23`
- Owner: `Software Engineer Agent`

## Change History
| Date | Version | Summary |
| --- | --- | --- |
| 2026-03-18 | v0.1 | Initial implementation plan for the CV++ v0.1 MVP. |
| 2026-03-18 | v0.2 | Marked Milestone 1 complete and resolved initial logging-path decisions. |
| 2026-03-18 | v0.3 | Started Milestone 2 with parser transparency logs, parsed summaries, and optional fixture capture support. |
| 2026-03-19 | v0.4 | Closed the initial high-resolution stream investigation for `profile2` and `profile4`. |
| 2026-03-19 | v0.5 | Reopened the high-resolution investigation after intermittent startup failures and metadata-branch concerns. |
| 2026-03-20 | v0.6 | Corrected the earlier profile-specific conclusion and shifted focus back to full-app orchestration. |
| 2026-03-23 | v0.7 | Closed the foundation milestone and prepared the next milestone around metadata evidence and performance visibility. |

## Summary
The project has moved beyond initial RTSP bring-up. The current baseline can receive video, capture metadata, parse multi-object frames, and render multi-object overlays. The next milestone should make metadata evidence and metadata performance directly visible to the user.

## Milestone 1: Minimal Runtime Skeleton
Status: completed.

Completed work:
- added a TOML config file for RTSP URL, headers, latency, and output root
- extracted config loading out of `main.cpp`
- added a session logger that writes plain text raw metadata lines to disk
- defined a normalized in-memory metadata object structure
- verified the project still builds with no UI redesign

## Milestone 2: Metadata Capture and Parse Transparency
Status: completed.

Completed work:
- raw metadata is logged before parsing
- parse status is classified as success, unknown-pattern, no-objects, or malformed-payload
- parsed summaries are written to `parsed_summary.log`
- optional real-session fixture candidate capture is supported through `config.toml`
- normal shutdown logs RTSP methods and confirms `PAUSE` plus `TEARDOWN`
- `metadata_probe` was added as a control experiment for camera behavior
- the app metadata session now consumes the selected auxiliary video track instead of selecting and dropping it
- fragmented metadata handling was improved enough to recover multi-object parsing in live sessions
- live runs now show simultaneous multi-object overlays again

Current verified outcome:
- both `profile2` and `profile4` can produce object-bearing metadata
- the app can parse and display multiple simultaneous objects
- recent sessions contain meaningful `Car`, `Human`, and `Bicycle` detections
- the earlier profile-specific metadata conclusion has been corrected

## Milestone 3: Metadata Evidence and Performance Visibility
Status: next.

Goal:
Make the runtime answer two questions directly:
1. Did the camera send metadata for the object currently in view?
2. How good is the camera's metadata performance over a session?

Tasks:
- add an evidence banner or panel showing raw metadata seen, parsed object count, overlay object count, and metadata age
- add session metrics for detections by type and unique object IDs by type
- distinguish repeated detection events from unique tracked objects using camera-reported `ObjectId`
- surface malformed payload rate and parser health in a human-checkable way
- ensure the user can tell whether missing overlay means missing metadata, parse loss, or display-state loss

Done when:
- the app exposes evidence state without requiring manual log inspection
- the user can compare repeated detections and unique object counts in one session
- the app makes it clear whether an object was not sent by the camera or simply not shown on screen

## Milestone 4: Overlay State Isolation
Goal: make freshness and stale-object behavior easier to trust and maintain.

Tasks:
- extract overlay state handling from `main.cpp`
- centralize freshness timeout and stale-clear rules
- verify disappearing objects are removed correctly against captured samples

## Milestone 5: Minimal Verification View
Goal: provide one-screen operator verification without expanding scope into product UI.

Tasks:
- show live video with overlay
- show recent parsed metadata summary
- show recent raw metadata lines or a raw metadata panel
- show connection and reconnect status

## Milestone 6: Basic Session Robustness
Goal: make v0.1 usable during normal camera instability.

Tasks:
- add simple automatic reconnect behavior
- log reconnect attempts and session state changes
- verify reconnect state is visible in the verification view

## Key Concerns
- Avoid a large refactor before observability is working.
- Do not build a polished UI before raw metadata evidence is trustworthy.
- Keep each milestone independently runnable.
- Use `metadata_probe` as the control experiment before drawing conclusions from the full app.

## Resolved Decisions
- default log path strategy: session-based runtime output folders under `output/session-YYYYMMDD-HHMMSS/`
- raw metadata log format for v0.1: single plain file per session
- `metadata_probe` is the control experiment for camera-behavior validation
- the next milestone should prioritize metadata evidence and metadata performance over visual polish

## Reference
- `docs/projects/CV++/HIGH_RESOLUTION_PROFILE_INVESTIGATION.md`
- `docs/projects/CV++/MILESTONE_REVIEW.md`
