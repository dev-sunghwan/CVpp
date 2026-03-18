# CV++ Tasks

## Document Control
- Version: `v0.2`
- Status: `Milestone 1 completed`
- Created: `2026-03-18`
- Last Updated: `2026-03-18`
- Owner: `Software Engineer Agent`

## Change History
| Date | Version | Summary |
| --- | --- | --- |
| 2026-03-18 | v0.1 | Initial implementation plan for the CV++ v0.1 MVP. |
| 2026-03-18 | v0.2 | Marked Milestone 1 complete and resolved initial logging-path decisions. |

## Summary
This plan follows the approved PM scope and Tech Lead architecture. The order is intentional: make configuration and raw metadata observability work first, then improve parser trust, then add the minimum verification UI surface needed for fast operator checks.

## Milestone 1: Minimal Runtime Skeleton
Status: completed.

Completed work:
- added a TOML config file for RTSP URL, headers, latency, and output root
- extracted config loading out of `main.cpp`
- added a session logger that writes plain text raw metadata lines to disk
- defined a normalized in-memory metadata object structure
- verified the project still builds with no UI redesign

Completed outcome:
- stream settings are no longer hardcoded
- the app can create a raw metadata log file
- config and logging failures are visible in console or logs
- each run creates a session-based output folder

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

## Resolved Decisions
- default log path strategy: session-based runtime output folders under `output/session-YYYYMMDD-HHMMSS/`
- raw metadata log format for v0.1: single plain file per session

## Recommendation
Move directly to Milestone 2. Milestone 1 is complete and the next practical risk is parser transparency rather than more configuration work.

## Open Questions
- Should parse failures be shown only in logs first, or also reflected in a minimal on-screen status line during Milestone 2?
- What is the smallest acceptable first fixture set from a real Hanwha camera session?

## Decision Request for SungHwan
Approve the transition from Milestone 1 to Milestone 2: metadata capture and parse transparency.
