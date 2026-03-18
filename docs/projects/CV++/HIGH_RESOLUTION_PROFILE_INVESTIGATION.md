# High-Resolution Profile Investigation

## Document Control
- Version: `v0.4`
- Status: `Partially resolved`
- Created: `2026-03-18`
- Last Updated: `2026-03-18`
- Owner: `Tech Lead Agent`

## Summary
The original high-resolution playback issue is resolved for `profile4`, but not yet for `profile2`.

What is now confirmed:
- high-resolution playback failure was not primarily a camera-side problem
- `profile4` works in the app after pipeline changes
- `profile2` is externally valid, but still does not reach usable playback inside the app

This means the broader issue should be treated as an app-side RTSP/GStreamer compatibility problem across profiles, with one working high-resolution baseline and one remaining profile-specific gap.

## External Validation
The following checks prove that the camera exposes valid high-resolution streams outside the app.

For `profile4`:
- VLC plays `rtsp://<user>:<password>@<host>/profile4/media.smp` successfully.
- `gst-launch-1.0` shows both:
  - `media=video`, `encoding-name=H264`, `a-framesize=1920-1080`
  - `media=application`, `encoding-name=VND.ONVIF.METADATA`

For `profile2`:
- `gst-discoverer-1.0` reports a live RTSP stream with:
  - H.264 High Profile video
  - width `1920`
  - height `1080`
  - frame rate `30/1`
  - ONVIF timed metadata present

This is the key conclusion:
- both `profile4` and `profile2` are valid high-resolution targets externally
- any remaining failure is in the application's handling path

## App-Side Results
Working result after the fix for `profile4`:
- `rtspsrc` exposes metadata and H.264 video pads
- the app links the video pad successfully
- `decodebin` produces `video/x-raw` at `1920x1080`
- the app receives the first video sample successfully

Representative app log for `profile4`:
- `Linked video pad to video_queue: H264`
- `decodebin pad-added: src_0 | caps=video/x-raw ... width=(int)1920, height=(int)1080`
- `First video sample received: 1920x1080`

Current result for `profile2` in the app:
- pipeline reaches `NULL -> READY -> PAUSED`
- no dynamic pad logs were observed in the startup capture
- no metadata samples or parsed summaries were written
- no first video sample arrived within 10 seconds

## Root Cause Analysis
### Confirmed root cause for `profile4`
The root cause for the original `profile4` failure was app-side pipeline robustness.

Concretely:
1. A working external probe used `rtspsrc -> queue -> decodebin`.
2. The app originally linked the RTSP video pad directly to `decodebin`.
3. That direct path was not robust enough for `profile4`.
4. After introducing `video_queue` and better pad diagnostics, `profile4` video playback worked correctly.

### Current interpretation for `profile2`
`profile2` is not explained by the original camera-profile hypothesis either, because external tools can inspect it successfully.

The leading hypotheses now are:
1. `profile2` still differs in timing or activation behavior in ways the app does not yet handle well.
2. The app may still be too sensitive to multi-stream session ordering or startup timing.
3. The current diagnostics are sufficient to prove the failure remains app-side, but not yet sufficient to isolate the exact trigger for `profile2`.

## Implemented Fixes
Fixes already applied in the app:
- add `video_queue` between `rtspsrc` and `decodebin`
- force RTSP transport over TCP
- keep `Bestshot` disabled
- keep `Rate-Control` disabled for this verification path
- log full caps and pad names for dynamic RTSP and decodebin pads
- log first received video sample resolution
- harden config parsing against UTF-8 BOM input

## What We Learned
### Technical lesson
A single working profile does not prove the pipeline is robust across profiles. A fix can solve one stream shape while still leaving another profile-dependent negotiation issue open.

### Debugging lesson
Use control experiments aggressively:
- VLC is a client-side control
- `gst-launch-1.0` and `gst-discoverer-1.0` are protocol/path controls
- the app is only one implementation of the stream consumer

A strong counterexample should change the hypothesis quickly. In this case:
- first correction: `profile4` was not a camera-definition problem
- second correction: `profile2` is also not simply an invalid profile

### Learning point for SungHwan
The real skill here is not memorizing GStreamer APIs. It is learning to separate:
- stream validity
- camera behavior
- app pipeline behavior
- evidence from interpretation

That separation is what prevented us from making the wrong fix for the wrong reason.

## Remaining Open Issue
The high-resolution issue should now be split into two states:
- resolved: `profile4` playback in the app
- open: `profile2` playback in the app

So the correct status is not "fully resolved" and not "camera issue". It is:
- high-resolution support is proven feasible in the app
- one additional high-resolution profile still needs targeted debugging

## Recommended Next Step
Treat `profile4` as the current high-resolution verification baseline and run a focused follow-up investigation for `profile2` only.

That follow-up should compare:
- app startup logs for `profile2` vs `profile4`
- whether metadata branch participation affects `profile2`
- whether startup timing, session ordering, or profile-specific negotiation is different enough to require another pipeline adjustment
