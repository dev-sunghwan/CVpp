# High-Resolution Profile Investigation

## Document Control
- Version: `v0.3`
- Status: `Resolved for profile4`
- Created: `2026-03-18`
- Last Updated: `2026-03-18`
- Owner: `Tech Lead Agent`

## Summary
The high-resolution playback issue was reproducible in the app and is now considered resolved for `profile4`. The camera was not the primary problem. External validation with VLC and `gst-launch-1.0` proved that `profile4` exposes both high-resolution H.264 video and ONVIF metadata. The main issue was the application's video pipeline shape.

## Final Findings
Known-good external evidence:
- VLC plays `rtsp://admin:Sunap1!!@192.168.4.225/profile4/media.smp` successfully.
- `gst-launch-1.0` shows both:
  - `media=video`, `encoding-name=H264`, `a-framesize=1920-1080`
  - `media=application`, `encoding-name=VND.ONVIF.METADATA`

Known-good app evidence after the fix:
- `rtspsrc` exposes both metadata and H.264 video pads.
- the app links the video pad successfully.
- `decodebin` produces `video/x-raw` at `1920x1080`.
- the app receives the first video sample successfully.

Representative app log after the fix:
- `Linked video pad to video_queue: H264`
- `decodebin pad-added: src_0 | caps=video/x-raw ... width=(int)1920, height=(int)1080`
- `First video sample received: 1920x1080`

## Root Cause
The root cause was app-side pipeline robustness, not camera-side profile validity.

More specifically:
1. The working external probe used a `rtspsrc -> queue -> decodebin` pattern.
2. The app originally linked the RTSP video pad directly into `decodebin`.
3. For `profile4`, that direct path was not reliable enough, even though the stream itself was valid.
4. After introducing `video_queue` and improving dynamic pad handling diagnostics, `profile4` video playback worked correctly in the app.

## Implemented Fixes
Code and configuration changes that materially contributed to the fix:
- add `video_queue` between `rtspsrc` and `decodebin`
- force RTSP transport over TCP
- keep `Bestshot` disabled
- keep `Rate-Control` disabled for this verification path
- log full caps and pad names for dynamic RTSP and decodebin pads
- log first received video sample resolution
- harden config parsing against UTF-8 BOM input

## Why Earlier Hypotheses Were Incomplete
Earlier hypotheses over-weighted camera-side causes because:
- `profile10` worked and `profile4` did not
- the app sometimes exposed only metadata for `profile4`
- `profile2` also behaved differently

What changed the direction was a strong counterexample:
- VLC playing `profile4` correctly
- GStreamer CLI also seeing the video track correctly

That evidence falsified the idea that `profile4` was simply metadata-only or misconfigured.

## Remaining Technical Notes
This does not mean all profile-related risk is gone.

Still open:
- `profile2` behavior remains unexplained
- overlay quality for fast-moving vehicles still needs investigation
- object metadata handling should still be separated more clearly from event/status metadata

But the original high-resolution playback blocker for `profile4` is no longer open.

## Learning Notes for SungHwan
This issue is a good example of practical debugging discipline.

Key lessons:
- Treat VLC and `gst-launch` as control experiments, not just convenience tools.
- A valid counterexample should change the leading hypothesis quickly.
- "Camera problem" and "app pipeline problem" are different classes of failure and should be tested separately.
- When a minimal external pipeline works, the next question is not "why is the stream broken?" but "what is different in our app path?"
- Reproducing a known-good topology inside the app can be a faster path than speculative tuning.

## Recommendation
Treat this issue as resolved for `profile4`, keep `profile4` available as the high-resolution verification baseline, and move subsequent work to overlay quality and metadata interpretation rather than RTSP stream acquisition.
