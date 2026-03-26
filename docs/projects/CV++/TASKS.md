# CV++ Tasks

## Document Control
- Version: `v1.3`
- Status: `Milestone 3 active; ONVIF pipeline validated on profile4/profile2 smoke; daytime operator validation pending`
- Created: `2026-03-18`
- Last Updated: `2026-03-26`
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
| 2026-03-23 | v0.8 | Added the agreed Qt plus SQLite transition track while keeping the current milestone active. |
| 2026-03-24 | v0.9 | Completed the first live Qt verification shell slice and narrowed the next step to Qt polish plus SQLite preparation. |
| 2026-03-25 | v1.0 | Added Hanwha metadata baseline analysis and SQLite storage requirements docs, and narrowed the next step to parser-health visibility plus SQLite foundation work. |
| 2026-03-25 | v1.1 | Added a deferred future milestone for thermal camera metadata validation. |
| 2026-03-25 | v1.2 | Documented the interim ONVIF metadata pipeline finding and added the remaining app-level verification step. |
| 2026-03-26 | v1.3 | Validated the ONVIF-aware metadata path on profile4 and profile2 smoke runs and added startup-stability follow-up work. |

## Summary
The project has moved beyond initial RTSP bring-up. The current baseline can receive video, capture metadata, parse multi-object frames, and render multi-object overlays. The active milestone remains metadata evidence and performance visibility, but the metadata transport baseline is now much clearer.

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
- fragmented metadata handling was improved enough to recover multi-object parsing in live sessions
- live runs now show simultaneous multi-object overlays again

Current verified outcome:
- both `profile2` and `profile4` can produce object-bearing metadata
- the app can parse and display multiple simultaneous objects
- the earlier profile-specific metadata conclusion has been corrected

## Milestone 3: Metadata Evidence and Performance Visibility
Status: in progress.

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

Completed in the first vertical slice:
- added a runtime evidence banner showing raw-seen, parsed-count, overlay-count, and metadata age
- added session-end metrics logging for detections by type and unique object IDs by type
- started tracking raw payload count, parsed payload count, malformed payload count, and event-only payload count in shared runtime state
- replaced the OSD-first view with a minimal verification layout: video on the left, evidence and metadata panels on the right
- added a runtime connection setup UI for IP, username, password, and profile input
- completed a first live Qt shell slice that can connect, show video, render overlay labels, and update evidence, metrics, and recent metadata panels from the shared runtime state

Completed in the current analysis and transport slice:
- documented the current Hanwha metadata baseline from saved sessions, including parser-health ratios, stable class baseline, and parser-noise observations
- documented the minimal SQLite storage requirements needed to turn session logs into local review data without replacing the raw log flow
- aligned metadata analysis docs with the current parser taxonomy: `truncated-object-fragment`, `recovered-continuation`, and `metadata-without-objects`
- verified with `metadata_probe` that an RTP-aware ONVIF metadata path (`rtpjitterbuffer -> rtponvifmetadatadepay -> appsink`) materially reduces malformed payloads compared with the old raw-RTP appsink path
- validated the ONVIF-aware metadata path in fresh full-app `profile4` and `profile2` smoke runs
- moved `MetadataRtspSession` to metadata-only selection so profile behavior is treated as a startup-stability question, not a metadata-semantics question
- introduced first-frame delay and link-aware metadata watchdog logic to reduce delayed overlay startup risk

Current verified outcome:
- `profile4` full-app smoke can reach `application/x-onvif-metadata` and clean parse results with `malformed-payload=0`
- `profile2` smoke can also reach `application/x-onvif-metadata` and clean parse results with `malformed-payload=0`
- parser-noise labels did not appear in the validated clean ONVIF-path smoke sessions

Next step inside Milestone 3:
- surface parser-health counts in the Qt shell using the current taxonomy
- reduce video-session startup retries further so startup latency is more repeatable
- package the current nighttime nonvisual smoke verification into a repeatable procedure
- keep session summaries aligned with the baseline docs so later SQLite ingestion can stay mechanical
- do a daytime object-rich validation pass for overlay responsiveness and operator confidence

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

## Milestone 7: Qt Verification UI Transition
Goal: move the operator-facing UI off the temporary OpenCV-only view and onto a maintainable desktop UI platform.

Tasks:
- define a small Qt shell application that hosts the existing runtime core
- keep RTSP session, parser, and logging modules reusable from the current C++ implementation
- replace the temporary connection setup canvas with a proper Qt connection form
- replace the current right-side panel rendering with Qt widgets for evidence, metrics, and recent metadata
- preserve the current verification workflow while improving readability, DPI handling, and layout quality

Completed work:
- installed Qt 6.8.3 MSVC 2022 64-bit locally
- added a separate `CVPP_QtShell` target to the build
- created a minimal Qt shell with a connection form area and a verification layout placeholder
- wired the Qt connection form to runtime-backed session startup
- connected the Qt shell to `VideoRtspSession`, `MetadataRtspSession`, and `SharedAppState`
- replaced the video placeholder with a live runtime frame surface and overlay preview
- updated evidence, metrics, and recent metadata panels from the shared runtime state

Next step inside the Qt transition:
- improve connection UX and state messaging
- tighten panel density and visual hierarchy
- keep the OpenCV view only as fallback while validating the Qt shell as the main operator surface

## Milestone 8: SQLite Session Review Foundation
Goal: add a practical local persistence layer for session metrics and later metadata review.

Tasks:
- define a minimal SQLite schema for session summaries, parsed detections, and unique object metrics
- persist session-end metrics alongside the existing plain text logs
- keep plain logs as raw evidence even after SQLite is added
- prepare the data model needed for future review and comparison screens

Completed in planning:
- drafted `docs/projects/CV++/SQLITE_STORAGE_REQUIREMENTS.md`
- narrowed the first schema to `sessions`, `session_artifacts`, `parsed_payloads`, `parsed_objects`, and `session_type_metrics`

Recommended order:
1. finish the Qt verification shell polish
2. deepen metadata performance metrics in the UI and session summary
3. start the SQLite foundation

## Milestone 9: Thermal Camera Metadata Validation
Status: deferred future track.

Goal: extend the verification workflow so CV++ can validate thermal-camera metadata once the current visible-light Hanwha observability path is stable.

Tasks:
- confirm RTSP and metadata delivery behavior from the target thermal camera
- capture representative thermal metadata fixtures and session logs
- document payload format and transport differences relative to the current Hanwha baseline
- verify whether the current parser normalization can be reused or whether a separate thermal metadata path is needed
- define the UI, review, and storage adjustments needed to compare visible-light and thermal metadata sessions

Start condition:
- begin only after the current Milestone 3 and Milestone 8 foundations are stable enough that thermal work does not blur the current observability baseline

## Key Concerns
- avoid a large refactor before observability is working
- do not build a polished UI before raw metadata evidence is trustworthy
- keep each milestone independently runnable
- use `metadata_probe` as the control experiment before drawing conclusions from the full app
- keep the implementation learnable enough that SungHwan can use the project to build practical C++ understanding

## Resolved Decisions
- default log path strategy: session-based runtime output folders under `output/session-YYYYMMDD-HHMMSS/`
- raw metadata log format for v0.1: single plain file per session
- `metadata_probe` is the control experiment for camera-behavior validation
- the next milestone should prioritize metadata evidence and metadata performance over visual polish
- medium-term UI direction: Qt desktop application
- medium-term storage direction: SQLite for local session review

## Reference
- `docs/projects/CV++/HIGH_RESOLUTION_PROFILE_INVESTIGATION.md`
- `docs/projects/CV++/HANWHA_METADATA_BASELINE_ANALYSIS.md`
- `docs/projects/CV++/MILESTONE_REVIEW.md`
- `docs/projects/CV++/SQLITE_STORAGE_REQUIREMENTS.md`
- `docs/projects/CV++/MESSAGE_INVESTIGATION.md`
- `docs/projects/CV++/ONVIF_METADATA_PIPELINE_INVESTIGATION.md`
- `docs/projects/CV++/METADATA_REFERENCE.md`
- `docs/projects/CV++/CPP_LEARNING_GUIDE.md`
