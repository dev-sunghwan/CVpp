# High-Resolution Profile Investigation

## Document Control
- Version: `v0.1`
- Status: `Open investigation`
- Created: `2026-03-18`
- Last Updated: `2026-03-18`
- Owner: `Tech Lead Agent`

## Summary
The current application can stream and overlay metadata successfully with `profile10`, but higher-resolution profiles are not yet reliable. This is now a distinct technical investigation track because long-term verification quality will be limited if only the low-resolution profile is usable.

## What We Know
Confirmed working baseline:
- `profile10` opens successfully
- video pad is exposed and decoded
- metadata pad is exposed
- video window appears and overlay draws

Observed failures:
- `profile2` did not progress beyond `READY -> PAUSED`
- `profile2` exposed no usable video frames in the app
- direct `gst-launch` probing also failed to show quick success for `profile2`
- `profile4` exposed metadata but no video pad

Important implementation changes already tested:
- `Bestshot` header disabled
- `Rate-Control` header disabled
- RTSP over TCP forced
- JPEG/MJPEG video pads accepted in addition to H264

## Current Hypotheses
Most likely causes, in priority order:
1. The camera profile itself is configured differently and may not actually expose a usable video track for the requested stream.
2. The camera may allow only limited concurrent consumers for some profiles.
3. Some profiles may require different transport or encoder settings than the current app assumes.
4. The issue may be in camera-side profile definition rather than in the app pipeline.

Less likely causes:
- basic GStreamer wiring, because `profile10` already proves the main app path works
- metadata path design, because `profile4` still delivered metadata separately

## Recommended Debugging Sequence
1. Verify the camera-side profile definitions for `profile2` and `profile4`.
   - Confirm codec, resolution, transport expectations, and whether video is enabled.
2. Test each profile with only one consumer connected.
   - Eliminate multi-client contention as a variable.
3. Probe profiles outside the app with minimal tools.
   - `gst-launch-1.0` against each profile
   - if available later, compare with `ffprobe` or vendor tools
4. Capture and compare RTSP negotiation results across profiles.
   - DESCRIBE/SETUP/PLAY behavior
   - whether dynamic video pads are ever emitted
5. Only after the above, reconsider app-side pipeline changes.
   - Do not overfit the app to a camera profile that may be misconfigured

## Practical Next Actions
Short-term actions for SungHwan:
- keep feature work moving with `profile10`
- inspect profile settings in the camera UI for `profile2` and `profile4`
- note codec, resolution, smart codec options, and any bestshot/event-related settings
- retry with no other clients connected

Short-term actions for engineering:
- keep `profile10` as the active development baseline
- separate object metadata handling from event metadata handling
- preserve the current diagnostics so profile investigation can continue later

## Why This Matters
If high-resolution profiles cannot be opened reliably, later evaluation work becomes harder:
- overlay trust is weaker on small frames
- metadata-to-image comparison is less useful
- future external CV comparison becomes less representative

This is a real risk, but it is not a blocker for all v0.1 progress. The right approach is to keep a working baseline while running a bounded investigation.

## Learning Notes for SungHwan
The value of this investigation is not only fixing the stream. The useful engineering lesson is learning how to separate:
- app bug vs camera/profile behavior
- product blocker vs parallel technical risk
- evidence, hypothesis, and next experiment

At this stage, "following along" is expected. The important skill is not memorizing every GStreamer detail immediately. It is learning how to narrow a problem with logs, controlled configuration changes, and explicit hypotheses.

## Recommendation
Continue development on `profile10`, but treat high-resolution profile support as an explicit architecture risk under investigation rather than an informal background issue.

## Decision Request for SungHwan
Approve this split strategy:
- development baseline remains `profile10`
- high-resolution profile support is investigated in parallel with explicit camera-side and RTSP-side checks