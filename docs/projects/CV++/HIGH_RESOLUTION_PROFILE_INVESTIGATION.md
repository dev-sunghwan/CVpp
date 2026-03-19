# High-Resolution Profile Investigation

## Document Control
- Version: `v0.6`
- Status: `Partially resolved`
- Created: `2026-03-18`
- Last Updated: `2026-03-19`
- Owner: `Tech Lead Agent`

## Change History
| Date | Version | Summary |
| --- | --- | --- |
| 2026-03-18 | v0.1 | Recorded the initial high-resolution playback issue and early hypotheses. |
| 2026-03-18 | v0.4 | Documented that `profile4` was fixed while `profile2` remained open. |
| 2026-03-19 | v0.5 | Verified graceful RTSP teardown and temporarily confirmed both `profile2` and `profile4` playback. |
| 2026-03-19 | v0.6 | Reopened the investigation after confirming intermittent `profile2` startup failures and isolating metadata-branch involvement. |

## Summary
The high-resolution playback issue is resolved for `profile4`, but only partially resolved for `profile2`.

What is confirmed now:
- both profiles are valid high-resolution RTSP targets outside the app
- `profile4` is a reliable in-app high-resolution baseline
- `profile2` is still intermittent in the mixed video-plus-metadata pipeline
- `profile2` becomes much more stable in video-only mode
- normal shutdown sends RTSP `PAUSE` and `TEARDOWN`

## Verified Evidence
External validation:
- VLC plays `rtsp://<user>:<password>@<host>/profile2/media.smp` successfully
- VLC plays `rtsp://<user>:<password>@<host>/profile4/media.smp` successfully
- `gst-discoverer-1.0` reports `1920x1080` H.264 plus ONVIF metadata for `profile2`
- SDP comparison showed `profile2` and `profile4` are effectively equivalent at the protocol-description level

App validation for `profile4`:
- `rtspsrc` exposes metadata and H.264 video pads
- the app links the H.264 pad successfully
- `decodebin` produces `video/x-raw` at `1920x1080`
- the first video sample arrives successfully

App validation for `profile2` in mixed mode:
- `OPTIONS`, `DESCRIBE`, `SETUP`, `PLAY`, `PAUSE`, and `TEARDOWN` are normal
- failures are intermittent rather than permanent
- some runs expose metadata pad only
- some runs expose video pad and raw decode output but still fail to deliver the first sample
- the startup watchdog can recover some failed runs by resetting the pipeline and retrying

App validation for `profile2` in video-only mode:
- when the metadata appsink is removed from the pipeline, `profile2` reaches `First video sample received: 1920x1080`
- repeated probes were materially more stable than the mixed-path runs

Representative evidence from the latest `profile2` debugging:
- mixed mode failure pattern: metadata pad appears, video pad never appears, startup retries exhausted
- mixed mode second failure pattern: video pad and `video/x-raw` pad appear, but no first sample arrives
- video-only success pattern: `Linked H264 video pad to video_queue.` then `First video sample received: 1920x1080`

## Root Cause Interpretation
The issue is app-side and runtime-sensitive.

What we know now:
1. The camera profiles are not invalid.
2. The problem is not explained by SDP differences.
3. The problem is not caused by teardown failures.
4. `profile2` has at least two intermittent startup failure patterns.
5. Metadata participation in the pipeline appears to make `profile2` startup less stable.

The current effective fixes and mitigations are:
- make the video path explicit: `rtspsrc -> queue -> rtph264depay -> h264parse -> decodebin`
- keep RTSP transport on TCP for this verification path
- disable empty RTSP headers instead of sending blank values
- set appsinks to `async=false` to reduce preroll sensitivity
- add startup watchdog retry logic with explicit `TEARDOWN` and replay
- support a true video-only mode by omitting the metadata appsink from the pipeline

## RTSP Session Shutdown Result
Normal shutdown is verified.

Confirmed behavior on exit:
- the app requests pipeline shutdown cleanly
- RTSP `PAUSE` is sent
- RTSP `TEARDOWN` is sent

This removes the earlier concern that repeated tests might leave camera sessions open because of the app's normal exit path.

## What This Investigation Taught Us
### Technical lesson
A profile can be valid, externally playable, and still be unstable in one specific app pipeline. That means the next step is to isolate participation by branch, sink, and state transition rather than keep questioning the camera.

### Debugging lesson
The useful sequence was:
1. confirm stream validity outside the app
2. compare protocol evidence instead of guessing from UI behavior
3. identify whether failure occurs before pad emission, before decode, or before first sample delivery
4. add recovery logic only after narrowing the failure boundary
5. split mixed-pipeline and video-only behavior instead of treating them as the same case

### Learning point for SungHwan
The valuable skill here is separating symptoms from layers:
- camera validity
- RTSP negotiation
- dynamic pad activation
- decode output
- first-sample delivery
- metadata branch interaction

That separation is what turned a vague intermittent bug into a smaller architectural question.

## Current Recommendation
Treat the high-resolution investigation as partially resolved.

Use this operating baseline:
- `profile4`: reliable mixed video-plus-metadata baseline
- `profile2`: reliable enough for video-only verification, but not yet reliable enough as a mixed pipeline baseline

The next discussion should focus on architecture, not guesswork. Specifically:
- whether `profile2` should use split video and metadata pipelines
- whether metadata should be consumed asynchronously from the stable video path
- whether the product should prefer `profile4` as the default mixed-mode baseline for now
