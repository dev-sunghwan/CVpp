# High-Resolution Profile Investigation

## Document Control
- Version: `v0.7`
- Status: `Reframed`
- Created: `2026-03-18`
- Last Updated: `2026-03-20`
- Owner: `Tech Lead Agent`

## Change History
| Date | Version | Summary |
| --- | --- | --- |
| 2026-03-18 | v0.1 | Recorded the initial high-resolution playback issue and early hypotheses. |
| 2026-03-18 | v0.4 | Documented that `profile4` was fixed while `profile2` remained open. |
| 2026-03-19 | v0.5 | Verified graceful RTSP teardown and temporarily confirmed both `profile2` and `profile4` playback. |
| 2026-03-19 | v0.6 | Reopened the investigation after confirming intermittent startup failures and metadata-branch involvement. |
| 2026-03-20 | v0.7 | Corrected the earlier profile-specific conclusion after clearing stale `gst-launch` sessions and rerunning controlled 1-minute probes. |

## Summary
The earlier conclusion that `profile2` and `profile4` had different metadata behavior should not be treated as valid.

What is confirmed now:
- both `profile2` and `profile4` are valid high-resolution RTSP targets
- both profiles produced object-bearing metadata in controlled 1-minute probe runs
- stale `gst-launch-1.0.exe` processes polluted earlier test results by holding camera sessions open
- the remaining problem is app-side orchestration and runtime behavior, not a confirmed per-profile camera rule
- normal shutdown sends RTSP `PAUSE` and `TEARDOWN`

## Verified Evidence
External validation:
- VLC plays `rtsp://<user>:<password>@<host>/profile2/media.smp` successfully
- VLC plays `rtsp://<user>:<password>@<host>/profile4/media.smp` successfully
- `gst-discoverer-1.0` reports `1920x1080` H.264 plus ONVIF metadata for both profiles
- SDP comparison showed no meaningful protocol-description difference that explains the earlier contradiction

Controlled probe validation on `2026-03-20`:
- `profile2` + `metadata-only`: object-bearing metadata observed
- `profile2` + `metadata-with-video`: object-bearing metadata observed
- `profile4` + `metadata-only`: object-bearing metadata observed
- `profile4` + `metadata-with-video`: object-bearing metadata observed after clearing stale `gst-launch` sessions and fixing the probe to consume the selected video pad

Main-app validation on `2026-03-20`:
- `VideoSession` reaches `First video sample received: 1920x1080`
- `MetadataSession` now links both metadata and auxiliary video pads when selected
- `MetadataSession` receives real metadata samples and parses object-bearing payloads in-session
- representative parsed objects include `Human` and `Car`

## Root Cause Interpretation
The camera behavior was mischaracterized because the test environment was dirty and one control tool was incomplete.

What we know now:
1. The camera profiles are not the root problem.
2. Earlier contradictory results were affected by leftover `gst-launch-1.0.exe` processes that kept RTSP sessions open.
3. The `metadata_probe` originally had the same class of bug as the main app: it could select a video track without consuming it.
4. After clearing stale sessions and consuming selected auxiliary video pads, both profiles produced metadata in controlled tests.
5. The real remaining work is in the full app: startup ordering, session coordination, and overlay trust.

## RTSP Session Shutdown Result
Normal shutdown is verified.

Confirmed behavior on exit:
- the app requests pipeline shutdown cleanly
- RTSP `PAUSE` is sent
- RTSP `TEARDOWN` is sent

This removes the earlier concern that the normal exit path itself was leaking camera sessions.

## What This Investigation Taught Us
### Technical lesson
When live RTSP behavior looks contradictory, first suspect the test harness and leftover sessions before inventing per-profile camera rules.

### Debugging lesson
The useful sequence was:
1. validate the stream outside the app
2. isolate behavior with a minimal probe
3. clear stale processes and sessions before trusting conclusions
4. make the probe consume every selected track
5. only then compare app behavior against the control probe

### Learning point for SungHwan
The key skill here is not guessing faster. It is building a control experiment, cleaning the environment, and refusing to generalize from dirty runs.

## Current Recommendation
Treat the camera-behavior question as substantially clarified.

Use this operating baseline:
- do not assume `profile2` and `profile4` have different metadata semantics without a clean repro
- treat leftover debug processes as a first-class source of false conclusions
- focus the next debugging cycle on the main app, especially startup ordering, session orchestration, and overlay trust
- keep `metadata_probe` as the control experiment whenever the full app behaves unexpectedly
