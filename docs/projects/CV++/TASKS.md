# CV++ Tasks

## Document Control
- Version: `v0.1`
- Status: `Draft`
- Created: `2026-03-18`
- Last Updated: `2026-03-18`
- Owner: `Software Engineer Agent`

## Change History
| Date | Version | Summary |
| --- | --- | --- |
| 2026-03-18 | v0.1 | Initial implementation plan for the CV++ v0.1 MVP. |

## Summary
This plan follows the approved PM scope and Tech Lead architecture. The order is intentional: make configuration and raw metadata observability work first, then improve parser trust, then add the minimum verification UI surface needed for fast operator checks.

## Milestone 1: Minimal Runtime Skeleton
Goal: make the app configurable and ready to log raw metadata with the smallest possible code movement.

Tasks:
- add a TOML config file for RTSP URL, headers, latency, and log path
- extract config loading out of `main.cpp`
- add a simple session logger that can write plain text raw metadata lines to disk
- define a normalized in-memory metadata event shape, even if parsing is still partial
- verify the app still builds and runs with no UI redesign

Done when:
- stream settings are no longer hardcoded
- the app can create a raw metadata log file
- failures in config loading or log creation are visible in console or logs

## Milestone 2: Metadata Capture and Parse Transparency
Goal: make raw metadata and parse results observable at the same time.

Tasks:
- route raw metadata payloads through logging before parsing
- surface parse success, parse failure, and unknown pattern cases explicitly
- capture a small real-camera sample set for fixtures
- add basic checks for representative samples: normal object, empty scene, disappearing object, variant class pattern

Done when:
- raw and parsed outputs can be compared for the same session
- parser failures no longer fail silently
- sample fixtures exist from real Hanwha metadata

## Milestone 3: Overlay State Isolation
Goal: make freshness and stale-object behavior easier to trust and maintain.

Tasks:
- extract overlay state handling from `main.cpp`
- centralize freshness timeout and stale-clear rules
- verify disappearing objects are removed correctly against captured samples

Done when:
- overlay updates are handled outside the main entry flow
- stale detections clear consistently

## Milestone 4: Minimal Verification View
Goal: provide one-screen operator verification without expanding scope into product UI.

Tasks:
- show live video with overlay
- show recent parsed metadata summary
- show recent raw metadata lines or a raw metadata panel
- show connection and reconnect status

Done when:
- the user can compare video, overlay, parsed output, and raw evidence in one screen

## Milestone 5: Basic Session Robustness
Goal: make v0.1 usable during normal camera instability.

Tasks:
- add simple automatic reconnect behavior
- log reconnect attempts and session state changes
- verify reconnect state is visible in the verification view

Done when:
- temporary stream interruptions are visible and the app attempts recovery automatically

## Key Concerns
- Avoid a large refactor before observability is working.
- Do not build a polished UI before raw metadata evidence is trustworthy.
- Keep each milestone independently runnable.

## Recommendation
Start with Milestone 1 immediately. It is the smallest step that reduces risk and unlocks the rest of the MVP.

## Open Questions
- Should the default log path live under the project directory or a runtime-generated output folder?
- Should raw metadata logs rotate in v0.1, or stay as single session files for simplicity?

## Decision Request for SungHwan
Approve this implementation order, especially the very small first milestone: config externalization plus plain-file raw metadata logging.
