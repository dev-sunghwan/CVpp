# CV++ C++ Learning Guide

## Purpose
Use this project as a practical C++ learning path, not just as a finished tool. The goal is to understand how a real runtime application is structured, verified, and debugged.

## What To Learn In Order
### 1. Program Structure
Read these files first:
- `main.cpp`
- `app_config.h` / `app_config.cpp`
- `shared_app_state.h`

Focus on:
- how configuration enters the app
- how shared state is passed around
- where the top-level runtime loop lives

### 2. Session Ownership
Read next:
- `video_rtsp_session.h` / `video_rtsp_session.cpp`
- `metadata_rtsp_session.h` / `metadata_rtsp_session.cpp`

Focus on:
- constructor dependencies
- who owns GStreamer elements
- startup, retry, and shutdown flow
- how session logic is separated by responsibility

### 3. Parsing And Normalization
Read next:
- `metadata_types.h`
- `metadata_parser.h` / `metadata_parser.cpp`

Focus on:
- how raw text is converted into typed objects
- how parse status is represented
- how malformed inputs are handled without crashing the app
- why canonical class normalization matters for metrics and UI clarity

### 4. Observability
Read next:
- `session_logger.h` / `session_logger.cpp`
- `metadata_probe.cpp`
- `tools/run_profile_smoke.ps1`

Focus on:
- how logs are used as evidence
- how a minimal control experiment differs from the full app
- how to debug assumptions with smaller tools
- how repeated smoke runs help separate transport quality from startup timing noise

### 5. Qt Verification Shell
Read after the runtime core makes sense:
- `qt_shell_window.h` / `qt_shell_window.cpp`
- `shared_app_state.h`
- `docs/projects/CV++/VERIFICATION_GUIDE.md`

Focus on:
- how runtime state is translated into operator-facing readiness and parser-health views
- why the UI now uses smaller functions such as snapshot capture, readiness calculation, and table updates
- how the Qt shell is used as a verification surface rather than as a polished product UI

## Suggested Read Path For The Current Qt Refactor
Use this exact order when studying the parser-health and readiness slice:
1. `shared_app_state.h`
   - find `ParserHealthCounts`
   - understand which fields are cumulative session evidence and which are latest-payload state
2. `metadata_rtsp_session.cpp`
   - start at `record_parser_health(...)`
   - then read where parse results update `last_parse_status_text`, parser-health counts, object state, and recent summaries
3. `qt_shell_window.cpp`
   - read `captureRuntimeSnapshot(...)`
   - then `buildReadinessViewModel(...)`
   - then the table update helpers
   - then `buildOperatorStatePanel(...)` and `buildVerificationPanel(...)`
   - end at `refreshUiFromState(...)`
4. `docs/projects/CV++/VERIFICATION_GUIDE.md`
   - compare the code paths with the operator meanings the UI is supposed to communicate

Why this order matters:
- the runtime state comes first
- the metadata session decides what evidence exists
- the Qt shell only translates that evidence into something a human can read quickly

## What This Refactor Teaches
### A. Function-level UI design
The recent Qt shell work is a good example of reducing ambiguity by splitting one large refresh path into smaller jobs:
- capture a runtime snapshot
- derive readiness meaning from that snapshot
- update each table from already-derived state
- render the frame separately from evidence and metrics

Learn to explain why this is better than one large UI function:
- easier to test mentally
- easier to change wording without breaking transport behavior
- easier to see whether a bug is in runtime state or UI interpretation

### B. Runtime evidence vs operator wording
The parser still produces forensic values such as `status` and `message`, but the operator view groups them into a smaller readiness and parser-health model.

Learn to explain the boundary:
- logs keep detailed parser evidence
- the UI keeps the shortest useful explanation
- changing the wording in the UI should not change the transport baseline itself

### C. When not to chase a known issue
The current project state is a good example of disciplined scope control.

Learn to explain why the team is not chasing every metadata startup delay right now:
- the ONVIF-aware metadata path is already validated
- `profile2` and `profile4` should not be treated as different metadata meanings
- first-arrival timing still varies, but the app can still reach valid metadata reception
- that makes startup variability a known non-blocking issue unless it starts blocking practical verification work again

## Pipeline Lessons To Learn From This Project
### A. RTP vs ONVIF metadata boundary
The major lesson from this project is that RTP packet boundaries and ONVIF metadata document boundaries are not the same thing.

Learn to explain:
- why `application/x-rtp` at `appsink` was too early for clean metadata parsing
- why `rtpjitterbuffer -> rtponvifmetadatadepay -> appsink` is the more correct transport path
- why parser-noise labels such as `Caur` can be caused by reading metadata at the wrong boundary

### B. Split sessions
Learn to explain why CV++ now prefers separate video and metadata sessions.

Focus on:
- why a stable video baseline matters more than a clever mixed graph
- why metadata startup should not be allowed to break video startup
- why `profile2` and `profile4` should be treated as startup-repeatability differences, not metadata-semantics differences

### C. Known issue discipline
Learn the difference between:
- a blocking architecture problem
- a known non-blocking startup issue

This project is a good example:
- the metadata transport path was a blocking architecture problem until the ONVIF-aware path was introduced
- startup repeatability remains worth tracking, but it is now a known non-blocking issue rather than the main blocker

## Recommended Learning Tasks
1. Trace one object from raw metadata to parsed object to on-screen overlay.
2. Explain why the ONVIF-aware metadata path is now the validated baseline.
3. Explain how `ParserHealthCounts` moves from metadata parsing into the Qt operator surface.
4. Pick one readiness label in the Qt shell and explain exactly which runtime conditions produce it.
5. Explain, in your own words, why `metadata_probe` exists.
6. Explain the difference between detection events and unique object IDs.
7. Compare one daytime validation session and one nighttime smoke summary and explain what each one can and cannot prove.

## Small Exercises For This Week
1. Add one temporary parser-health row to the Qt shell, rebuild, and then remove it after you understand the data flow.
2. Change one readiness phrase in the Qt shell and verify that only wording changed, not runtime behavior.
3. Read one `parsed_summary.log` session and match three lines to what the Operator State panel would have shown.
4. After a live run, write down whether a metadata delay was a blocker or only a non-blocking startup variation, and justify the answer from evidence.

## C++ Concepts You Can Learn Here
- structs and data ownership
- references and constructor injection
- mutex and shared state protection
- lifecycle management around external libraries
- translating raw input into typed internal models
- modular debugging instead of single-file debugging
- evidence-based runtime validation instead of guess-based tweaking

## Practical Rule
When you feel lost, do not read the whole project at once.

Use this loop:
1. pick one question
2. identify the one file most responsible
3. follow data in and out of that file
4. verify the result in logs or runtime behavior

That is the main C++ learning habit this project should teach.
