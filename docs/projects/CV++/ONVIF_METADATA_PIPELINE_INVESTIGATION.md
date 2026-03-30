# CV++ ONVIF Metadata Pipeline Investigation

## Document Control
- Version: `v0.3`
- Status: `Transport validated; startup repeatability remains a known non-blocking issue`
- Created: `2026-03-25`
- Last Updated: `2026-03-27`
- Owner: `Software Engineer Agent`

## Purpose
This document records how the CV++ metadata pipeline was narrowed from a raw RTP heuristic path to an ONVIF-aware transport path, what full-app smoke validation established for `profile4` and `profile2`, and what remains as a known non-blocking startup issue.

## Why This Investigation Was Needed
Saved sessions showed repeated signs of payload fragmentation and label corruption such as:
- `malformed-payload`
- `XML start marker not found`
- partial object recovery sequences
- parser-noise labels such as `Caur`, `Caar`, `Cayr`, and `Hukuman`

That raised a practical question:
- is the camera itself sending broken object labels?
- or is the app reading RTP-framed ONVIF metadata too early and treating transport chunks like complete XML documents?

## Initial Pipeline Problem
The earlier metadata path linked the metadata RTP pad directly into `appsink`.

Observed metadata pad caps in saved sessions:
- `application/x-rtp`
- `media=application`
- `encoding-name=VND.ONVIF.METADATA`

Interpretation:
- the app was not receiving already reassembled ONVIF metadata documents
- it was receiving RTP-framed ONVIF metadata samples
- the app then tried to recover XML boundaries itself by searching for `<?xml` and appending later chunks with `pending_xml_fragment_`

That means the previous behavior depended on an application-level XML-boundary heuristic instead of a metadata-aware RTP depayload path.

## ONVIF Direction Check
The ONVIF direction is correct.

Local environment validation confirmed:
- `rtponvifmetadatadepay` is available
- its sink caps accept `application/x-rtp` with `encoding-name=VND.ONVIF.METADATA`
- its source caps output `application/x-onvif-metadata`

This matches the expected direction for ONVIF RTP metadata handling.

## Probe Comparison
Comparison target:
- same camera
- same `profile4`
- same probe duration: `20s`

### Old raw-RTP probe path
Path meaning:
- raw RTP metadata samples delivered directly to `appsink`

Observed appsink caps:
- `application/x-rtp`

Observed summary:
- `metadata_samples=406`
- `object_payloads=398`
- `event_only=8`
- `malformed=200`

Typical behavior:
- repeated alternation between `malformed-payload` and `success`
- partial objects where one object first appeared as `Vehicle` in a malformed step and then `Car` after recovery

### RTP-aware ONVIF probe path
Path meaning:
- `rtpjitterbuffer -> rtponvifmetadatadepay -> appsink`

Observed appsink caps:
- `application/x-onvif-metadata`

Observed summary:
- `metadata_samples=206`
- `object_payloads=198`
- `event_only=8`
- `malformed=0`

Typical behavior:
- object-bearing payloads arrived as direct `success`
- the previous malformed/recovery alternation disappeared in the probe run

## Current Interpretation
Current evidence supports this conclusion:
- a meaningful portion of the earlier fragmentation behavior came from the CV++ metadata capture path, not only from the camera
- the previous app path was reading RTP-framed ONVIF metadata too early
- using a metadata-aware ONVIF depayload path moves the appsink boundary from RTP packets to ONVIF metadata buffers
- that change alone was enough to drive probe-level malformed count from `200` to `0`

## Full-App Validation: Profile4
Fresh full-app session used for initial validation:
- session: `output/session-20260325-160520/`
- target: `profile4`
- runtime target logged as `rtsp://.../profile4/media.smp`
- metadata path logged as `RTP jitterbuffer + ONVIF metadata depay path`
- first metadata appsink caps logged as `application/x-onvif-metadata`

Observed parsed summary distribution:
- `success=19847`
- `no-objects=287`
- `malformed-payload=0`
- `noise_detections=0`

Interpretation:
- the improved ONVIF path is not only a probe success
- the full app also reached a clean object-bearing parse path once metadata started flowing
- earlier parser-noise labels did not appear in the validated `profile4` full-app run

## Full-App Smoke Validation: Profile2
Initial profile2 smoke run showed:
- first video sample arrived
- metadata session started
- metadata pad link did not happen before the earlier watchdog threshold
- retry was needed before the first metadata sample arrived

After startup-stability tuning, a fresh profile2 smoke session showed:
- session: `.tmp_profile2_smoke/output/session-20260326-001245/`
- `VideoSession: first video sample received: 1920x1080`
- `MetadataSession: starting pipeline`
- `MetadataSession: linked metadata RTP pad to jitterbuffer`
- `MetadataSession: first appsink sample caps=application/x-onvif-metadata`
- `MetadataSession: first metadata sample received`
- no metadata watchdog retry in that run

Observed parsed summary distribution:
- `success=28`
- `no-objects=16`
- `malformed-payload=0`

Interpretation:
- `profile2` does not need a different metadata meaning model
- it can follow the same ONVIF-aware pipeline direction as `profile4`
- the remaining difference is startup timing sensitivity, not metadata semantics

## Repeatability Conclusion
Repeated smoke benchmarking is now available via:
- `tools/run_profile_smoke.ps1`

Practical conclusion from repeated smoke work:
- the ONVIF-aware metadata transport path is a usable baseline
- startup repeatability, especially on the video side and especially for `profile4`, is still variable
- this variability should be treated as a known non-blocking startup issue, not as a sign that the metadata transport direction is wrong

## Updated Practical Conclusion
The core metadata-transport conclusion is now strong enough to treat as validated for the current runtime direction:
- earlier fragmentation and type-label corruption were materially amplified by the old app-side RTP handling path
- the ONVIF-aware depayload path is the correct runtime direction
- making the metadata session metadata-only was necessary for the full app to realize the same benefit seen in the probe
- `profile2` and `profile4` should be treated as startup repeatability differences, not metadata meaning differences

## Remaining Follow-Up
The following still remains:
- daytime object-rich validation for overlay responsiveness and operator confidence
- turning the current nonvisual smoke verification into a small repeatable verification procedure
- improving startup repeatability, especially on the video side, if it later becomes a real blocker
- confirming that the same startup behavior remains stable across repeated runs, not only single smoke sessions

## Recommendation
Treat the ONVIF-aware metadata pipeline as the validated baseline. Treat startup repeatability as a known non-blocking issue for now, continue with UI evidence improvements and verification workflow cleanup, and revisit startup tuning only if it becomes a practical blocker.
