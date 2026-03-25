# CV++ Milestone Review

## Document Control
- Version: `v0.3`
- Status: `Updated`
- Created: `2026-03-23`
- Last Updated: `2026-03-25`
- Owner: `PM / Tech Lead / Software Engineer Agents`

## Summary
The current milestone should still be treated as a successful foundation milestone, but the team now has a clearer metadata-analysis baseline for what comes next. Recent work clarified the real product goal, corrected earlier false conclusions about profile-specific metadata behavior, stabilized the RTSP control path enough for continued development, restored multi-object overlay behavior in the main app, and documented the current Hanwha parser-health baseline plus the SQLite storage requirements for later review.

## What Was Achieved
- externalized RTSP configuration into `config.toml`
- added session-based logging under `output/session-.../`
- captured raw metadata and parsed summaries for each session
- confirmed graceful RTSP shutdown with `PAUSE` and `TEARDOWN`
- created `metadata_probe` as a control experiment for camera behavior
- corrected the earlier wrong conclusion that `profile2` and `profile4` had different metadata semantics
- aligned the full app metadata session with the control probe by consuming selected auxiliary video pads
- restored multi-object parsing and multi-object overlay in live runs
- documented the Hanwha metadata baseline from saved sessions
- documented the minimal SQLite storage requirements for local session review

## Key Findings
- the camera profiles themselves were not the main issue
- stale `gst-launch` sessions and incomplete probe paths produced misleading conclusions
- live metadata often arrives fragmented, so parser and session logic must tolerate partial payloads
- the main app can now receive and render multiple simultaneous objects
- object counts seen in logs are detection events, not unique object counts
- the newer parser taxonomy is a relabeling of already-observed continuation behavior, not a new camera condition
- SQLite planning can stay small because the current runtime already emits the session and parsed-summary data needed for a first local review layer

## Current Verified State
The app can now:
- receive RTSP video from the Hanwha camera
- receive ONVIF/WiseAI metadata
- log raw metadata and parsed summaries
- render overlays for multiple simultaneous objects
- show that `Car`, `Human`, and `Bicycle` are all present in real sessions
- launch a Qt verification shell with connection form, live frame view, evidence, metrics, and recent metadata panels
- explain the current Hanwha baseline with saved-session evidence instead of relying on ad-hoc observations

## Remaining Gaps
- the Qt shell is viable now, but it still needs UI polish before it can fully replace the temporary OpenCV view
- metadata performance is still measured mostly as repeated detections, not camera-reported unique objects or durations
- fragmented payloads can still distort type labels and per-frame object completeness
- parser-health breakdown is still mostly in logs and document analysis, not yet fully surfaced in the runtime UI
- session review is still log-first; SQLite-backed review is defined now, but not implemented yet

## Decision
Close the current milestone as a foundation and observability milestone.

Open the next milestone with a tighter product goal: make metadata evidence, parser health, and metadata performance explicit in the runtime experience.

## Next Milestone Goal
The next milestone should turn the Qt shell into the primary verification surface and answer two user questions directly:
1. Did the camera actually send metadata for the object I am looking at?
2. How good is the camera's metadata performance over a session?

That means the next milestone should add:
- a polished Qt verification layout showing evidence, metrics, and recent metadata clearly
- an operator-facing parser-health breakdown built on the current message taxonomy
- session metrics for detections by type and unique object IDs by type, with room for object continuity / duration views
- a clear distinction between repeated detection events and unique tracked objects
- lightweight summary output that helps SungHwan evaluate camera metadata quality without manual log digging
- preparation for SQLite-backed session review without replacing the current runtime core

## Recommendation
Do not pivot away from the current architecture. Keep the current C++/GStreamer runtime core, treat the Qt shell as the new presentation baseline, and focus the next milestone on Qt polish, parser-health visibility, metadata performance reporting, and SQLite preparation.

## Learning Outcome
This milestone also established a useful learning baseline for SungHwan:
- how a C++ desktop runtime is split into config, session, parser, state, and logging responsibilities
- how to debug a live system by separating control experiments from the full app
- how camera behavior, parser behavior, and UI behavior must be distinguished before drawing conclusions
- how saved session outputs can be turned into an evidence-backed baseline and then into concrete storage requirements
