# CV++ Tasks

## Document Control
- Version: `v0.4`
- Status: `Milestone 2 in progress`
- Created: `2026-03-18`
- Last Updated: `2026-03-19`
- Owner: `Software Engineer Agent`

## Change History
| Date | Version | Summary |
| --- | --- | --- |
| 2026-03-18 | v0.1 | Initial implementation plan for the CV++ v0.1 MVP. |
| 2026-03-18 | v0.2 | Marked Milestone 1 complete and resolved initial logging-path decisions. |
| 2026-03-18 | v0.3 | Started Milestone 2 with parser transparency logs, parsed summaries, and optional fixture capture support. |
| 2026-03-19 | v0.4 | Closed the parallel high-resolution stream investigation for `profile2` and `profile4`. |

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
Status: in progress.

Implemented so far:
- raw metadata is logged before parsing
- parse status is classified as success, unknown-pattern, no-objects, or malformed-payload
- parsed summaries are written to `parsed_summary.log`
- optional real-session fixture candidate capture is supported through `config.toml`
- the current parse status is shown as a minimal on-screen banner
- high-resolution playback is verified for both `profile2` and `profile4`
- normal shutdown now logs RTSP methods and confirms `PAUSE` plus `TEARDOWN`

Remaining work:
- collect the first representative fixture set from a real Hanwha session
- verify that unknown pattern cases are surfaced as expected on real metadata
- separate object overlay logic from non-object event metadata
- improve overlay behavior for fast-moving vehicles

Done when:
- raw and parsed outputs can be compared for the same session
- parser failures no longer fail silently
- sample fixtures exist from real Hanwha metadata
- object overlay behavior is trustworthy enough for live verification

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
- high-resolution verification baseline: both `profile2` and `profile4` are valid in-app targets

## Recommendation
Keep Milestone 2 focused on live parser transparency, real metadata samples, and better object-overlay trust. Do not expand into reconnect or layout work yet.

## Parallel Investigation
The original high-resolution stream investigation is now closed.

Reference:
- `docs/projects/CV++/HIGH_RESOLUTION_PROFILE_INVESTIGATION.md`

## Open Questions
- Should real fixture capture stay opt-in through config, or become enabled by default during development?
- What is the smallest acceptable first fixture set from a real Hanwha camera session?
- What is the right freshness or hold behavior for fast-moving vehicles?

## Decision Request for SungHwan
Approve continuing Milestone 2 by collecting real metadata fixtures, removing non-object overlay noise, and improving fast-object overlay behavior on the live Hanwha stream.
