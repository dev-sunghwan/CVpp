# CV++ Decisions

## Document Control
- Version: `v0.2`
- Status: `Draft for approval`
- Created: `2026-03-18`
- Last Updated: `2026-03-18`
- Owner: `PM Agent`

## Change History
| Date | Version | Summary |
| --- | --- | --- |
| 2026-03-18 | v0.1 | Initial PM decision document defining user purpose, MVP scope, out of scope, and success criteria. |
| 2026-03-18 | v0.2 | Added PM decision for verification UI and mitigation direction for key concerns. |

## Summary
CV++ should be treated as a practical metadata-observability product first, not a general AI video platform yet. The immediate user value is letting SungHwan verify what a Hanwha RTSP stream is sending, whether custom headers work as intended, and whether parsed metadata is trustworthy enough to support later comparison with external CV models.

## User Purpose
The primary user is SungHwan as a developer/operator working with Hanwha cameras and NVRs.

The product should help the user:
- connect to a Hanwha RTSP stream from C++
- inject and validate custom RTSP headers
- inspect raw ONVIF-style metadata alongside video
- confirm that parsed objects and overlays reflect the real stream accurately

## MVP Scope
The MVP includes:
- stable RTSP video streaming from a target Hanwha device
- configurable RTSP URL, headers, and latency settings
- separate capture of video and metadata streams
- raw metadata capture for inspection
- human-readable parsed metadata summary
- on-screen overlay showing current detected objects
- metadata freshness handling so stale objects are not displayed

The MVP should stay focused on observability and validation, not advanced analytics.

## Out of Scope
For the MVP, do not include:
- external AI model inference such as YOLO, SAM, or face recognition
- large-scale storage, search, or analytics pipelines
- multi-camera fleet management
- production cloud deployment
- polished end-user UI beyond what is needed for operator verification
- fully generalized support for every RTSP vendor

## Success Criteria
The MVP is successful if:
- the user can run the app against a Hanwha RTSP source without editing hardcoded values
- custom headers can be configured and verified in the RTSP flow
- raw metadata can be captured and reviewed for debugging
- parsed metadata is visible in a concise, human-checkable form
- overlays match recent metadata and clear correctly when objects disappear
- the codebase is ready for the next step of comparing camera metadata with external CV outputs

## Verification UI Decision
UI is in scope for the MVP only as a verification interface, not as a polished product UI.

The preferred MVP interface is a simple operator-facing desktop screen that shows:
- live video
- current overlay result
- recent parsed metadata summary
- metadata freshness or timestamp state
- access to raw metadata capture or recent raw metadata lines

This is the PM decision: choose the UI that reduces verification time and ambiguity, not the UI that looks most complete. If a minimal desktop window can present video, parsed results, and raw metadata evidence together, that is sufficient for v0.

## Key Concerns
- Regex-based metadata parsing may remain brittle until replaced or constrained more clearly.
- A single-file structure will slow iteration if observability features are added without refactoring.
- Without sample metadata fixtures, regressions in parsing and overlay behavior will be hard to detect.

## Mitigation Decisions
### 1. Metadata parsing brittleness
Short-term response:
- keep regex parsing only for the MVP if it remains transparent and debuggable
- log parsing failures and unknown patterns explicitly
- keep raw metadata available beside parsed output so the user can compare them quickly

Mid-term response:
- move toward a more structured XML parsing approach once enough real samples are collected

### 2. Single-file implementation risk
Short-term response:
- do not attempt a large refactor immediately
- extract only the highest-churn responsibilities first: config loading, metadata parsing, overlay state, and logging
- keep the app runnable at every step

Mid-term response:
- shift from one large entry file toward small modules with clear responsibilities

### 3. Lack of sample metadata fixtures
Short-term response:
- treat real camera raw metadata as the source of truth
- capture a small set of representative samples from the Hanwha device
- preserve samples for at least normal objects, empty scenes, disappearing objects, and variant class patterns

Mid-term response:
- use those captured samples as repeatable fixtures for parser and overlay regression checks

## Recommendation
Approve CV++ as an MVP focused on RTSP control and metadata observability for Hanwha environments. Defer AI comparison features until metadata capture, parsing, and verification are reliable.

## Decision Request for SungHwan
Approve this product direction: "CV++ v0 is a Hanwha-focused C++ RTSP and metadata verification tool with a minimal verification UI, not yet an AI comparison platform."
