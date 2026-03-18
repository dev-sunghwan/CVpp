# CV++ Architecture

## Document Control
- Version: `v0.2`
- Status: `Ready for approval`
- Created: `2026-03-18`
- Last Updated: `2026-03-18`
- Owner: `Tech Lead Agent`

## Change History
| Date | Version | Summary |
| --- | --- | --- |
| 2026-03-18 | v0.1 | Initial lightweight architecture for the CV++ verification-focused MVP. |
| 2026-03-18 | v0.2 | Resolved initial architecture choices for logging, reconnect behavior, and config format. |

## Summary
For v0.1, the architecture should stay as a single-process desktop application in C++ built around GStreamer for RTSP transport and OpenCV for frame rendering and overlay drawing. The goal is not a general platform yet. It is a practical verification tool for Hanwha RTSP streams, custom headers, and metadata observability.

## Proposed v0.1 Architecture
Use one executable with a small number of internal modules:

```text
Config
  -> RtspSession
      -> VideoPipeline
      -> MetadataPipeline
  -> MetadataParser
  -> OverlayState
  -> VerificationView
  -> Logging
```

Module responsibilities:
- `Config`: load RTSP URL, headers, latency, and output options from a file.
- `RtspSession`: own pipeline startup, shutdown, reconnect, and shared runtime state.
- `VideoPipeline`: receive decoded frames and expose them as `cv::Mat`.
- `MetadataPipeline`: receive raw metadata payloads and forward them to logging and parsing.
- `MetadataParser`: convert raw XML or text payloads into a normalized internal object list.
- `OverlayState`: hold only fresh objects and clear stale detections safely.
- `VerificationView`: show video, overlays, parsed summary, and recent raw metadata evidence in one operator-facing screen.
- `Logging`: persist raw metadata samples, parser failures, and session events.

## Data Flow
1. `Config` loads stream and header settings.
2. `RtspSession` builds the GStreamer pipeline and injects custom RTSP headers.
3. `VideoPipeline` emits frames for display.
4. `MetadataPipeline` emits raw metadata payloads.
5. `Logging` stores raw payloads before parsing.
6. `MetadataParser` produces normalized detections and parse status.
7. `OverlayState` updates current visible objects based on freshness rules.
8. `VerificationView` renders video plus overlay and shows parsed/raw evidence side by side.

## Practical Technology Choices
- Language: C++
- Streaming: GStreamer with `rtspsrc` and `before-send`
- Frame and overlay handling: OpenCV
- Config format: TOML
- Persistence for v0.1: plain file logging first; SQLite only if raw history becomes immediately necessary
- Connection recovery for v0.1: simple automatic reconnect with visible status and session logs

## PM Decisions Still Worth Confirming
The MVP direction is already clear enough to start, but these PM decisions would reduce churn:
- exact verification UI layout priority: raw metadata panel always visible or toggleable
- minimum supported operator workflow: live verification only, or live plus post-run review
- success threshold for “good enough” parser coverage on early Hanwha samples

## Key Concerns
- Regex parsing is acceptable only if parse failures are explicit and raw payloads are preserved.
- A large refactor is not needed now, but `main.cpp` should stop owning every responsibility.
- If the verification view becomes too ambitious, UI work will slow core metadata validation.

## Recommendations
- Start with a modular monolith, not multiple processes or services.
- Log raw metadata first, then parse, then render.
- Keep the verification UI simple and evidence-oriented.
- Add sample raw metadata fixtures from real cameras as soon as capture works.

## Open Questions
- Should the raw metadata panel stay always visible in the default verification view, or be collapsible?
- Is live verification alone sufficient for v0.1, or should the operator also review saved logs inside the app?
- What parser coverage threshold is acceptable for the first real Hanwha sample set?

## Decision Request for SungHwan
Approve a modular single-process C++ desktop architecture for v0.1, with GStreamer transport, OpenCV rendering, file-based config, raw metadata logging, and a one-screen verification UI.
