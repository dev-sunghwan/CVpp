# CV++ Architecture

## Document Control
- Version: `v0.5`
- Status: `Approved with split-pipeline and Qt transition direction`
- Created: `2026-03-18`
- Last Updated: `2026-03-23`
- Owner: `Tech Lead Agent`

## Change History
| Date | Version | Summary |
| --- | --- | --- |
| 2026-03-18 | v0.1 | Initial lightweight architecture for the CV++ verification-focused MVP. |
| 2026-03-18 | v0.2 | Resolved initial architecture choices for logging, reconnect behavior, and config format. |
| 2026-03-18 | v0.3 | Marked the current architecture as approved. |
| 2026-03-19 | v0.4 | Updated the architecture to prefer split video and metadata handling after profile2 mixed-pipeline instability findings. |
| 2026-03-23 | v0.5 | Added the agreed medium-term UI and storage transition direction: Qt desktop UI plus SQLite-backed session review. |

## Summary
For v0.1, the architecture should remain a single-process desktop application in C++ using GStreamer and OpenCV. However, the runtime strategy should now prefer a split structure: keep video playback as the stable baseline, and handle metadata in a separate path that cannot destabilize the video path.

This is still a practical verification tool for Hanwha RTSP streams, custom headers, and metadata observability. It is not yet a general AI video platform.

## Recommended Runtime Direction
Use one executable with small internal modules, but do not treat one mixed RTSP graph as the only baseline.

```text
Config
  -> SessionCoordinator
      -> VideoRtspSession
      -> MetadataRtspSession
  -> MetadataParser
  -> OverlayState
  -> VerificationView
  -> Logging
```

## Why The Split Direction Is Recommended
Recent debugging showed:
- `profile4` is stable in mixed mode
- `profile2` is intermittent in mixed mode
- `profile2` is materially more stable in video-only mode
- metadata participation in the same pipeline can increase startup instability

That means a mixed graph is still acceptable as an experiment path, but it is not a safe architecture baseline for all profiles.

## Module Responsibilities
- `Config`: load RTSP URL, headers, latency, output options, and metadata enablement from a file.
- `SessionCoordinator`: own startup, shutdown, retry, and high-level synchronization between video and metadata sessions.
- `VideoRtspSession`: prioritize stable decoded frames and expose them as `cv::Mat`.
- `MetadataRtspSession`: receive raw metadata payloads, log them, and forward them to parsing without being allowed to break video startup.
- `MetadataParser`: convert raw XML or text payloads into a normalized internal object list.
- `OverlayState`: hold only fresh objects and clear stale detections safely.
- `VerificationView`: show video, overlays, parsed summary, and recent raw metadata evidence in one operator-facing screen.
- `Logging`: persist raw metadata samples, parser failures, RTSP method logs, startup watchdog retries, and session events.

## Data Flow
1. `Config` loads stream and header settings.
2. `SessionCoordinator` starts the stable video session first.
3. `VideoRtspSession` establishes the video baseline and emits frames for display.
4. `MetadataRtspSession` starts separately and emits raw metadata payloads.
5. `Logging` stores raw payloads before parsing.
6. `MetadataParser` produces normalized detections and parse status.
7. `OverlayState` updates current visible objects based on freshness rules.
8. `VerificationView` renders video plus overlay and shows parsed/raw evidence side by side.

## Practical Technology Choices
- Language: C++
- Streaming: GStreamer with `rtspsrc` and `before-send`
- Frame and overlay handling: OpenCV for the current baseline, with Qt as the preferred next UI layer
- Config format: TOML
- Persistence for v0.1: plain file logging first; SQLite is the preferred next storage layer for session metrics and review
- Connection recovery for v0.1: simple startup retry plus visible status and session logs

## UI Platform Direction
The current OpenCV-based verification view is acceptable as a transitional interface, but it should not be treated as the long-term UI platform.

Agreed direction:
- keep the current C++ runtime, GStreamer sessions, parser, and logging core
- treat the OpenCV verification view as a temporary operator console
- evolve the presentation layer toward a Qt desktop application when the next level of UI quality and review workflow is needed

Why Qt is the preferred next step:
- the product is still local and desktop-oriented
- it needs forms, panels, metrics tables, and session review workflows
- it can reuse the current C++ core more directly than a browser-first rewrite
- it is a better fit than OpenCV for readable text, layout, and operator interaction

Browser-first direction remains deferred because it would imply a much larger architectural change: service boundaries, web transport, and multi-user workflow assumptions that the product does not yet require.

## Storage Direction
Plain logs remain the current evidence baseline, but the next realistic persistence layer should be SQLite.

Why SQLite fits the current product stage:
- it supports local session review and object-level history without infrastructure overhead
- it works well with a future Qt desktop application
- it is enough for storing session summaries, parsed objects, and unique object metrics

Server-hosted databases should be revisited only when the project clearly moves into multi-camera or multi-user workflows.

## Mixed Pipeline Policy
Treat the mixed pipeline as optional, not foundational.

Use it when:
- a profile is already known to be stable in mixed mode
- the reduced implementation complexity is worth it for that profile

Do not depend on it as the universal architecture baseline because current evidence shows profile-dependent instability.

## Tradeoffs
Benefits of the split direction:
- video stability is protected from metadata instability
- debugging boundaries become much clearer
- retries and recovery can be tuned separately
- profile-specific behavior becomes easier to isolate

Costs of the split direction:
- timestamp alignment becomes more explicit work
- session management is slightly more complex
- reconnect and health state need coordination across two paths

At this stage, those costs are acceptable because the product is still a verification tool. Stability and observability matter more than architectural elegance.

## SE Handoff Guidance
The next implementation steps should be:
1. keep `profile4` as the mixed-mode baseline for ongoing validation
2. keep `profile2` video-only as the stable diagnostic baseline
3. refactor the current runtime into `VideoRtspSession` and `MetadataRtspSession` responsibilities, even if both still live in one process
4. make overlay updates depend on parsed metadata availability, not on metadata pipeline ownership of the video graph
5. preserve the startup watchdog for unstable sessions
6. avoid re-coupling metadata startup to video startup during refactoring

## Key Concerns
- Regex parsing is acceptable only if parse failures are explicit and raw payloads are preserved.
- `main.cpp` should continue shrinking in responsibility.
- Do not overbuild synchronization logic before the metadata behavior is stable enough to justify it.

## Recommendation
Approve the split direction as the runtime baseline and Qt plus SQLite as the medium-term product evolution path.

In practical terms:
- runtime stability work should continue inside the current C++ and GStreamer core
- OpenCV UI work should remain tactical, not strategic
- future implementation should separate stable video transport from metadata transport as much as possible inside the app
- the next substantial UI investment should go into a Qt desktop layer
- the next substantial storage investment should go into SQLite-backed session review

## Open Questions
- Should `profile2` metadata use a fully separate RTSP session, or a separately managed branch within the same process?
- What timestamp alignment strategy is sufficient for v0.1: latest-metadata-wins, bounded freshness window, or explicit timestamp matching?
- When should the team retire mixed mode entirely for unstable profiles?

## Decision Request for SungHwan
Approved architecture direction: keep a modular single-process C++ desktop application, but move the runtime baseline toward split video and metadata handling so metadata cannot destabilize high-resolution video verification.
