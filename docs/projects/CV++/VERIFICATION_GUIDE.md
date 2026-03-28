# CV++ Verification Guide

## Purpose
Use CV++ as a camera metadata verification tool, not as an AI detector. The app should tell the operator what the camera sent, what the parser recovered, and what is currently shown on screen.

## Start A Session
When the app starts, use the connection setup window to enter:
- camera IP address
- username
- password
- profile number

The runtime RTSP URL is built as:
- `rtsp://<user>:<password>@<ip>/profile<profile>/media.smp`

For normal testing:
- use `profile4` for the current primary operator-facing validation path
- use `profile2` when you want to compare startup repeatability or cross-check profile behavior

## Screen Layout
- Left: live RTSP video with metadata overlay
- Right / Operator State: runtime readiness plus grouped parser-health counts
- Right / Evidence: current metadata state for the frame you are looking at
- Right / Session Metrics: cumulative detections and unique camera-reported object IDs
- Right / Recent Metadata: recent parsed summaries for quick spot checks

## How To Read The Operator State Panel
- `runtime`: overall stage for the current session such as `starting video`, `awaiting metadata`, `live`, or `metadata only`
- `video`: whether the first frame is still pending, already ready, or currently retrying
- `metadata`: whether metadata is still waiting on the video baseline, starting, waiting for the first payload, or actively receiving
- `parser`: simplified parser-health category derived from the latest parsed summary

## How To Read Parser Health Counts
- `clean object payload`: object-bearing payloads that ended cleanly
- `recovered continuation`: continuation chains that recovered usable object data
- `fragmented object payload`: payloads that still ended mid-object
- `continuation chunk`: continuation traffic that arrived without a fresh XML start marker
- `metadata without objects`: valid metadata traffic with no overlayable object blocks
- `unknown object pattern`: payloads where object metadata existed but parser coverage stayed incomplete

## How To Read The Evidence Panel
- `raw=seen/not-seen`: whether raw metadata payloads have arrived
- `parsed=N`: how many objects the parser recovered from the latest payload
- `overlay=N`: how many objects are currently rendered on the video
- `age=...ms`: time since the last raw metadata payload
- `fresh=yes/no`: whether the current overlay is still inside the freshness window

## How To Interpret Missing Overlay
- `raw=not-seen`: the camera did not send usable metadata recently
- `raw=seen`, `parsed=0`: metadata arrived, but the latest payload did not contain usable objects
- `parsed>0`, `overlay=0`: metadata was parsed, but the overlay is stale or not currently active
- `parsed>0`, `overlay>0`: the camera sent object metadata and the app is showing it

## How To Read Session Metrics
- `Detections by type`: repeated detection events across the session
- `Unique object IDs`: unique camera-reported `ObjectId` values seen in the session

Use both numbers together:
- high detections + low unique IDs usually means the same object stayed in view for a long time
- low detections + low unique IDs may mean the camera rarely emitted object metadata

## Daytime Object-Rich Validation
Use this when:
- the Qt shell wording or layout changed
- parser-health or readiness messaging changed
- you want to confirm operator confidence in a real object-rich scene

Preferred setup:
- start with `profile4` as the main daytime validation path
- use a scene with repeated pedestrian or vehicle movement, not a mostly empty FoV
- keep the smoke procedure separate; this pass is about operator interpretation, not startup benchmarking

Checklist:
1. Confirm the session reaches `runtime=live`, `video=ready`, and `metadata=receiving` before trusting overlay behavior.
2. Watch one object enter the FoV and confirm `raw=seen`, `parsed>0`, and `overlay>0` all become understandable without opening logs.
3. When parser-health changes to `recovered continuation` or `fragmented object payload`, confirm the recent metadata line still explains what happened at a glance.
4. Keep one object in view long enough to compare `Detections by type` versus `Unique object IDs` and confirm the difference still feels intuitive.
5. Let one object leave the FoV and confirm the operator can tell the difference between stale overlay clearance and metadata absence by checking Operator State plus Evidence.
6. At session end, review `session.log` and `parsed_summary.log` only as confirmation, not as the primary way to understand what happened live.

Pass criteria:
- the operator can explain a missing overlay in one quick read of Operator State, Evidence, and Recent Metadata
- object entry and exit behavior are understandable without log-first debugging
- parser-health wording feels specific enough that follow-up tuning is about copy or layout, not about hidden state

Follow-up triggers:
- `runtime` or `metadata` wording feels ambiguous during a normal object appearance
- parser-health counts move in a way that is hard to relate to the recent metadata list
- the operator still needs raw logs to distinguish metadata absence, parse loss, and overlay-state loss
## Nonvisual Smoke Verification
Repeated startup and transport checks can be run without operator interaction by using:
- `tools/run_profile_smoke.ps1`

What the script measures per run:
- first video sample arrival
- first metadata sample arrival
- video retry count
- metadata retry count
- parsed summary status counts

Use this when:
- you want to compare `profile2` and `profile4`
- you want to check startup repeatability
- you want to verify that the ONVIF-aware metadata path still reaches clean parse results after changes

Current practical interpretation:
- clean ONVIF-path smoke results are enough to treat the transport path as validated
- startup delay or retry can still exist as a known non-blocking issue without invalidating the transport baseline

## Recommended Verification Workflow
1. Confirm the video stream is stable.
2. Watch the Evidence panel while an object is in the FoV.
3. If overlay is missing, check whether `raw` and `parsed` changed.
4. Use Recent Metadata to confirm object type and object count.
5. At session end, review `session.log`, `parsed_summary.log`, and Session Metrics together.
6. When needed, run the repeated smoke script to compare startup behavior across profiles before assuming a transport regression.





