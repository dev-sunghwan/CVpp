# High-Resolution Profile Investigation

## Document Control
- Version: `v0.5`
- Status: `Resolved for profile2 and profile4`
- Created: `2026-03-18`
- Last Updated: `2026-03-19`
- Owner: `Tech Lead Agent`

## Change History
| Date | Version | Summary |
| --- | --- | --- |
| 2026-03-18 | v0.1 | Recorded the initial high-resolution playback issue and early hypotheses. |
| 2026-03-18 | v0.4 | Documented that `profile4` was fixed while `profile2` remained open. |
| 2026-03-19 | v0.5 | Verified graceful RTSP teardown and confirmed both `profile2` and `profile4` play correctly in the app. |

## Summary
The high-resolution playback issue is now resolved for both `profile2` and `profile4`.

What is confirmed now:
- both profiles are valid high-resolution RTSP targets outside the app
- both profiles now reach `1920x1080` playback inside the app
- normal shutdown sends RTSP `PAUSE` and `TEARDOWN`
- the root problem was app-side robustness, not a broken camera profile

## Verified Evidence
External validation:
- VLC plays `rtsp://<user>:<password>@<host>/profile2/media.smp` successfully
- VLC plays `rtsp://<user>:<password>@<host>/profile4/media.smp` successfully
- `gst-discoverer-1.0` reports `1920x1080` H.264 plus ONVIF metadata for `profile2`
- SDP comparison showed `profile2` and `profile4` are effectively equivalent at the protocol-description level

App validation for `profile4`:
- `rtspsrc` exposes metadata and H.264 video pads
- the app links the H.264 pad to the video branch successfully
- `decodebin` produces `video/x-raw` at `1920x1080`
- the first video sample arrives successfully

App validation for `profile2`:
- `OPTIONS`, `DESCRIBE`, `SETUP`, and `PLAY` complete successfully
- `rtspsrc` exposes metadata and H.264 video pads
- the app links the H.264 pad to the video branch successfully
- `decodebin` produces `video/x-raw` at `1920x1080`
- the first video sample arrives successfully

Representative evidence from the verified `profile2` run:
- `Linked H264 video pad to video_queue.`
- `decodebin pad-added: src_0 | caps=video/x-raw ... width=(int)1920, height=(int)1080`
- `First video sample received: 1920x1080`

## Root Cause Interpretation
The issue should be interpreted as an app-side runtime robustness problem.

What we learned from the investigation:
1. The camera profiles were not invalid. External clients could already play them.
2. The app path was more fragile than the external control paths.
3. Fixing one profile did not prove the entire path was stable.
4. Empty RTSP header values were also unsafe and needed to be filtered out.

The effective fix set was:
- make the video path more explicit and robust: `rtspsrc -> queue -> rtph264depay -> h264parse -> decodebin`
- keep RTSP transport on TCP for this verification path
- disable empty RTSP headers instead of sending blank values
- improve dynamic-pad and first-sample diagnostics
- verify normal shutdown behavior with RTSP method logging

## RTSP Session Shutdown Result
Normal shutdown is now verified.

Confirmed behavior on exit:
- the app requests pipeline shutdown cleanly
- RTSP `PAUSE` is sent
- RTSP `TEARDOWN` is sent

This removes the earlier concern that repeated tests might leave camera sessions open because of the app's normal exit path.

## What This Investigation Taught Us
### Technical lesson
When two profiles look identical in SDP but only one works, the next step is not to guess harder. The next step is to compare runtime behavior and harden the consumer path.

### Debugging lesson
The useful sequence was:
1. confirm the issue outside the app with control clients
2. compare protocol evidence instead of guessing from UI behavior
3. narrow the failure boundary step by step
4. change one part of the pipeline at a time
5. verify shutdown, not just startup

### Learning point for SungHwan
The main skill here was not GStreamer syntax. It was learning how to separate:
- camera validity
- protocol validity
- app pipeline behavior
- confirmed evidence
- interpretation

That separation prevented a wrong conclusion and led to the actual fix.

## Current Recommendation
Treat both `profile2` and `profile4` as valid high-resolution verification targets.

The next engineering focus should move away from stream activation and toward overlay quality:
- object-only metadata handling
- fast-moving vehicle tracking quality
- freshness and hold behavior
