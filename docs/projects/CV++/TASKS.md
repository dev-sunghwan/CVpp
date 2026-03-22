# CV++ Tasks

## Document Control
- Version: `v0.6`
- Status: `Milestone 2 in progress`
- Created: `2026-03-18`
- Last Updated: `2026-03-20`
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

## Summary
This plan follows the approved PM scope and Tech Lead architecture. The order remains the same: make configuration and raw metadata observability work first, then improve parser trust, then make the full verification runtime trustworthy enough for live use.

## Milestone 1: Minimal Runtime Skeleton
Status: completed.

Completed work:
- added a TOML config file for RTSP URL, headers, latency, and output root
- extracted config loading out of `main.cpp`
- added a session logger that writes plain text raw metadata lines to disk
- defined a normalized in-memory metadata object structure
- verified the project still builds with no UI redesign

## Milestone 2: Metadata Capture and Parse Transparency
Status: in progress.

Implemented so far:
- raw metadata is logged before parsing
- parse status is classified as success, unknown-pattern, no-objects, or malformed-payload
- parsed summaries are written to `parsed_summary.log`
- optional real-session fixture candidate capture is supported through `config.toml`
- the current parse status is shown as a minimal on-screen banner
- normal shutdown logs RTSP methods and confirms `PAUSE` plus `TEARDOWN`
- startup watchdog logic exists for failed starts
- a minimal `metadata_probe` control tool now exists
- the full app metadata session now consumes the selected auxiliary video track instead of selecting and dropping it

Current runtime reality:
- both `profile2` and `profile4` can produce object-bearing metadata in clean probe runs
- the earlier profile-specific metadata conclusion was wrong
- the current bottleneck is full-app orchestration and overlay trust, not a confirmed camera-profile rule
- the full app now receives metadata again after aligning one key path with the control probe

Remaining work:
- verify overlay behavior in the full app against the corrected metadata path
- reduce object-loss caused by fragmented or malformed metadata payloads
- collect the first representative fixture set from a real Hanwha session
- verify that unknown pattern cases are surfaced as expected on real metadata
- improve overlay behavior for fast-moving vehicles

Done when:
- raw and parsed outputs can be compared for the same session
- parser failures no longer fail silently
- sample fixtures exist from real Hanwha metadata
- the full app behaves consistently with the control probe
- object overlay behavior is trustworthy enough for live verification

## Milestone 3: Overlay State Isolation
Goal: make freshness and stale-object behavior easier to trust and maintain.

Tasks:
- extract overlay state handling from `main.cpp`
- centralize freshness timeout and stale-clear rules
- verify disappearing objects are removed correctly against captured samples

## Milestone 4: Minimal Verification View
Goal: provide one-screen operator verification without expanding scope into product UI.

Tasks:
- show live video with overlay
- show recent parsed metadata summary
- show recent raw metadata lines or a raw metadata panel
- show connection and reconnect status

## Milestone 5: Basic Session Robustness
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

## Recommendation
Keep Milestone 2 focused on making the full app match the now-correct probe behavior before adding more product behavior. Do not expand into reconnect or layout work yet.

## Parallel Investigation
Reference:
- `docs/projects/CV++/HIGH_RESOLUTION_PROFILE_INVESTIGATION.md`

## Open Questions
- What is the smallest acceptable first fixture set from a real Hanwha camera session?
- What is the right freshness or hold behavior for fast-moving vehicles?
- Why does the full app still diverge from `metadata_probe` under some runs even after the corrected camera conclusion?
