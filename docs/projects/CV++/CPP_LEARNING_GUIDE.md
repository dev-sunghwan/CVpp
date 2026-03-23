# CV++ C++ Learning Guide

## Purpose
Use this project as a practical C++ learning path, not just as a finished tool. The goal is to understand how a real runtime application is structured and debugged.

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

### 4. Observability
Read next:
- `session_logger.h` / `session_logger.cpp`
- `metadata_probe.cpp`

Focus on:
- how logs are used as evidence
- how a minimal control experiment differs from the full app
- how to debug assumptions with smaller tools

## Recommended Learning Tasks
1. Trace one object from raw metadata to parsed object to on-screen overlay.
2. Add one new metric and verify it in `session.log`.
3. Change one UI label in the verification view and rebuild.
4. Explain, in your own words, why `metadata_probe` exists.
5. Explain the difference between detection events and unique object IDs.

## C++ Concepts You Can Learn Here
- structs and data ownership
- references and constructor injection
- mutex and shared state protection
- lifecycle management around external libraries
- translating raw input into typed internal models
- modular debugging instead of single-file debugging

## Practical Rule
When you feel lost, do not read the whole project at once.

Use this loop:
1. pick one question
2. identify the one file most responsible
3. follow data in and out of that file
4. verify the result in logs or runtime behavior

That is the main C++ learning habit this project should teach.
