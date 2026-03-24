# CV++ Qt Transition Plan

## Document Control
- Version: `v0.2`
- Status: `In Progress`
- Created: `2026-03-23`
- Last Updated: `2026-03-24`
- Owner: `Tech Lead / Software Engineer Agents`

## Purpose
Define a practical transition from the current OpenCV-based verification UI to a Qt desktop UI without rewriting the RTSP and metadata runtime core.

## Scope
This plan is only about the operator-facing application shell:
- connection setup
- live verification layout
- evidence panels
- session metrics panels
- future session review panels

The following should stay reusable:
- `VideoRtspSession`
- `MetadataRtspSession`
- `MetadataParser`
- logging and probe logic

## Recommended Direction
Keep the current runtime core in C++, and introduce Qt as a presentation layer around it.

```text
Qt App Shell
  -> Connection Form
  -> Live Verification Window
  -> Evidence / Metrics Panels
  -> Future Session Review Panels

Runtime Core
  -> SessionCoordinator
  -> VideoRtspSession
  -> MetadataRtspSession
  -> MetadataParser
  -> OverlayState
  -> Logging
```

## Why This Is The Preferred Path
- it improves font quality, DPI handling, and layout without discarding the current runtime work
- it fits a local operator workflow better than a browser-first application
- it prepares the project for SQLite-backed session review without forcing a larger service architecture

## Transition Phases
### Phase 1
- create a minimal Qt shell window
- replace the current connection setup canvas with a Qt form
- embed the live verification layout in Qt widgets

Current status:
- `CVPP_QtShell` now builds and launches successfully
- the shell already includes a connection form, live frame surface, overlay preview, evidence panel, metrics panel, and recent metadata panel
- the runtime bridge is no longer the blocker; the next focus is UI polish and operational refinement

### Phase 2
- move evidence, metrics, and recent metadata into dedicated Qt panels
- reduce `main.cpp` UI responsibility further
- keep the current runtime control logic intact

Current status:
- evidence, metrics, and recent metadata are already wired into Qt widgets from `SharedAppState`
- the current task is to make the Qt shell readable and trustworthy enough to become the default operator surface

### Phase 3
- add SQLite-backed session review panels
- support session summary browsing and object-level inspection

## Tradeoffs
Benefits:
- much better readability and maintainability for operator UI
- cleaner forms, panels, and review workflow
- stronger platform for later metrics and session history

Costs:
- higher build and dependency complexity
- UI threading and runtime integration need care
- short-term implementation cost is larger than continuing with OpenCV-only rendering

## Recommendation
Treat Qt as the active UI transition, not a future experiment. Keep the current C++/GStreamer runtime core, use the OpenCV view only as a fallback while validating the Qt shell, and prepare the next persistence step with SQLite once the shell polish is good enough.
