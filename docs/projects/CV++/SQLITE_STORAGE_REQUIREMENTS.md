# CV++ SQLite Storage Requirements

## Document Control
- Version: `v0.2`
- Status: `First write-path slice implemented and runtime-validated`
- Created: `2026-03-25`
- Last Updated: `2026-04-01`
- Owner: `Software Engineer Agent`

## Implementation Status
Implemented in the first Milestone 8 foundation slice:
- local database path: `output/cvpp_review.db`
- `sessions` rows are created at session start
- `session_artifacts` rows are created at session start
- `parsed_payloads` and `parsed_objects` rows are appended during metadata handling
- `sessions` summary fields and `session_type_metrics` are updated on graceful session shutdown
- plain logs remain on disk even when SQLite writes succeed

Runtime-validated outcome from the first completed graceful session check:
- one completed session row was found with non-null `ended_at`
- summary fields such as `raw_samples`, `parsed_payloads`, `event_only_payloads`, and `detection_events` were populated
- `session_type_metrics` rows were written for the normalized types present in the session

Known current limitation:
- the first slice writes and validates review data, but it does not yet provide a dedicated in-app read or browsing surface

## Purpose
This document defines the minimum SQLite requirements for Milestone 8 so CV++ can move from log-only session review to practical local review without replacing the current plain-file evidence flow.

## Current Runtime Artifacts
Each session currently writes:
- `session.log`
  - lifecycle events
  - RTSP method logs
  - parser-health messages
  - session-end metrics summaries
- `metadata_raw.xml.log`
  - timestamped raw XML payload evidence
- `parsed_summary.log`
  - timestamped parsed status and object summaries

Current completed-session summary lines already include:
- `raw_samples`
- `parsed_payloads`
- `malformed`
- `event_only`
- `detection_events`
- `detections_by_type`
- `unique_ids_by_type`

## Product Questions SQLite Must Support
- Which sessions had the best and worst metadata quality?
- Did a session contain object metadata, event-only metadata, or mostly fragmented payloads?
- How many detections happened per type in one session?
- How many unique camera-reported object IDs appeared per type in one session?
- Which parsed payloads produced which objects?
- Which session folder and plain logs contain the raw evidence for a given reviewed session?

## Scope For Milestone 8
SQLite should be:
- local only
- single-user
- file-backed
- additive to the current log flow, not a replacement for it

SQLite should not yet become:
- the raw XML source of truth
- a multi-camera fleet database
- a cloud or server-backed system
- a large event warehouse

## Required Non-Functional Constraints
- keep plain logs on disk even after SQLite persistence is added
- do not store credentials such as RTSP passwords
- keep the schema simple enough to inspect manually with a desktop SQLite viewer
- allow one session folder to map cleanly to one SQLite session row
- keep inserts append-only during runtime where practical

## Required Core Entities
### `sessions`
One row per session folder.

Required fields:
- `session_id`
  - derived from folder name such as `session-20260324-163207`
- `started_at`
- `ended_at`
- `output_dir`
- `camera_host`
- `profile_name`
- `metadata_enabled`
- `raw_samples`
- `parsed_payloads`
- `malformed_payloads`
- `event_only_payloads`
- `detection_events`
- `notes`
  - optional free-text field for later operator review

Important rule:
- do not store the full RTSP URL with embedded credentials

### `session_artifacts`
Maps the session row to the existing plain logs.

Required fields:
- `session_id`
- `artifact_type`
  - expected initial values: `session_log`, `raw_metadata_log`, `parsed_summary_log`
- `artifact_path`

Purpose:
- lets review tools jump from SQLite summary data back to raw evidence

### `parsed_payloads`
One row per parsed summary line.

Required fields:
- `payload_id`
- `session_id`
- `observed_at`
- `parse_status`
- `parse_message`
- `object_count`
- `has_video_analytics`
  - derived or backfilled when available
- `is_continuation_related`
  - derived from parse message for v0.1

Why this table is required:
- Milestone 3 and the Hanwha baseline already depend on parser-health distributions, not only final detection totals

### `parsed_objects`
One row per parsed object occurrence.

Required fields:
- `parsed_object_id`
- `payload_id`
- `session_id`
- `object_id`
- `normalized_type`
- `likelihood`
- `left`
- `top`
- `right`
- `bottom`

Purpose:
- supports session review, unique-object metrics, and later timeline or filter views

### `session_type_metrics`
One row per session and normalized type.

Required fields:
- `session_id`
- `normalized_type`
- `detection_count`
- `unique_object_count`

Purpose:
- mirrors the current session-end metrics lines directly
- keeps the review query path simple even though these values are derivable from `parsed_objects`

## Recommended Minimal Schema Shape
Recommended first-pass tables:
1. `sessions`
2. `session_artifacts`
3. `parsed_payloads`
4. `parsed_objects`
5. `session_type_metrics`

Reasoning:
- this is enough to cover the current milestone questions
- it avoids a premature split into many small lookup tables
- it stays aligned with the log files the runtime already writes

## Derived Metrics SQLite Must Make Easy
SQLite-backed review should make these calculations easy:
- malformed-payload rate per session
- event-only rate per session
- detection events by type
- unique object IDs by type
- sessions with parser-noise labels outside the stable normalized class set
- sessions where metadata was present but clean payloads were rare

## Data Ownership Rules
- raw XML remains in `metadata_raw.xml.log`
- SQLite stores review data and structured summaries
- if structured data and logs disagree, logs remain the evidence baseline for debugging

## Ingestion Expectations
For the first implementation:
- session-start row can be created when `SessionLogger` initializes
- parsed payload and parsed object rows can be appended as metadata arrives
- final session summary fields can be updated on normal shutdown
- `session_type_metrics` can be written at session end from the already-tracked runtime aggregates

If runtime writes fail:
- the app should continue writing plain logs
- SQLite persistence should degrade gracefully rather than blocking the session

## Acceptance Criteria
Milestone 8 storage foundation is done when:
- each completed session can be found in SQLite by `session_id`
- SQLite can show session summary counts without reading the text logs
- SQLite can answer detections by type and unique IDs by type for a session
- SQLite can list parsed payload status and message counts for a session
- SQLite rows link back to the plain log files in the matching session folder
- raw XML logs still exist unchanged beside the database-backed review data

## Deferred For Later
- storing full raw XML blobs inside SQLite
- multi-session object tracking across cameras
- duration estimation for one `ObjectId`
- review annotations beyond a simple session note field
- database synchronization or remote storage

## Recommendation
Implement SQLite as a local review index over the existing session folders, not as a replacement for the logging system. The first schema should stay deliberately small and reflect the runtime data the app already knows how to produce today.



