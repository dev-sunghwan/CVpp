# CV++ Failure Case Studies

## Document Control
- Version: `v0.1`
- Status: `Draft baseline for recurring runtime failures`
- Created: `2026-03-30`
- Last Updated: `2026-03-30`
- Owner: `Tech Lead Agent`

## Purpose
This document records the two most important recurring failure patterns in CV++ so the team can stop rediscovering them from scratch.

The intent is not only to debug current issues, but also to preserve a learning-oriented case study for SungHwan.

## Scope
This first draft focuses on two high-value cases:
1. RTSP startup / first-frame failure
2. Metadata present but overlay missing

These are treated as recurring diagnostic categories, not one-off bugs.

## Why This Document Exists
Recent project history showed a repeated pattern:
- one visible symptom could come from multiple different causes
- the team sometimes spent time chasing the symptom before separating the layer where the real failure lived
- the same class of confusion can return unless the failure mode, current interpretation, and next checks are written down explicitly

This document should become the first stop when either of these two runtime problems appears again.

## Case 1: RTSP Startup / First-Frame Failure
### Problem Definition
The app starts a session, but the video stream does not reach a usable first frame quickly enough for normal verification work.

### Typical Symptoms
- connection attempt starts, but live video stays blank
- video may appear only after delay or retry
- metadata behavior becomes hard to interpret because the video baseline is missing
- `profile2` and `profile4` may appear to behave differently even when the underlying issue is startup timing sensitivity

### Why This Case Is Important
Without a stable first frame, the operator cannot trust later judgments about metadata timing, overlay timing, or profile behavior.

This is a transport and session-readiness problem first, not an overlay problem.

### Wrong Conclusions The Team Must Avoid
- assuming that a delayed first frame means the metadata architecture is wrong
- assuming that `profile2` and `profile4` have different metadata semantics
- assuming that one successful or failed startup run is enough to characterize the transport path

### Current Interpretation
Current project baseline:
- the ONVIF-aware metadata path is validated
- startup repeatability still varies
- this variability is currently treated as a known non-blocking issue unless it starts blocking practical verification work again

### What Is Already Known
- mixed pipeline behavior was not a safe universal baseline
- split video and metadata handling improved debugging boundaries
- repeated smoke runs are more useful than one-off threshold tweaking
- first-arrival timing can vary even when the runtime later becomes usable

### Current Working Hypothesis
This case is mainly about session startup sensitivity, retry timing, and transport readiness variability.

It should be treated separately from metadata meaning or parser correctness.

### What To Check First
1. Did the video session ever produce a first frame?
2. Did startup succeed only after a retry?
3. Was the session eventually usable even if startup was delayed?
4. Did the smoke workflow show transport validation despite variable startup timing?

### Evidence Sources
- `session.log`
- `tools/run_profile_smoke.ps1`
- Qt Operator State (`runtime`, `video`, `metadata`)

### Representative Session Walkthrough
Representative startup-delay example:
- session: `output/session-20260328-151752/`
- target: `profile4`
- `parsed_summary.log`: empty (`0` bytes)

Observed sequence:
- video did not produce a first sample within the initial `10s` watchdog window
- video retry triggered once
- first video sample arrived only after the retry
- metadata session started afterward, but metadata pad link still never happened before the metadata watchdog exhausted its retries
- the session ended with usable video having appeared late, but no metadata evidence having been captured in that run

Why this example matters:
- it looks like a profile-specific metadata problem if read too quickly
- but the actual evidence is more consistent with layered startup sensitivity: first the video path was late, then the metadata path also missed its startup window
- because the metadata transport direction was already validated elsewhere, this session should be read as a startup-repeatability case, not as proof that the ONVIF-aware metadata approach failed
### Current Product Decision
Treat this as a tracked runtime case, not as the current roadmap driver.

If the session becomes usable after delay and metadata later flows correctly, this remains a known non-blocking startup issue.

### Learning Note For SungHwan
This case teaches an important engineering habit:
- separate startup readiness problems from deeper architecture problems
- do not generalize from one run
- use repeated evidence before changing architecture direction

## Case 2: Metadata Present But Overlay Missing
### Problem Definition
The app is running, and metadata traffic may exist, but the expected overlay is not visible or disappears in a way that is hard to explain quickly.

### Typical Symptoms
- `raw=seen`, but no visible overlay
- `parsed>0`, but `overlay=0`
- overlay appears, then disappears, but the operator cannot tell why
- metadata panels and overlay behavior look inconsistent at a glance

### Why This Case Is Important
This is the most important user-facing ambiguity in the current milestone.

The product promise is not just to show overlays. It is to explain whether overlay absence means:
- missing metadata
- parse loss
- display-state loss or stale clear

### Wrong Conclusions The Team Must Avoid
- blaming parser failure immediately whenever overlay is missing
- treating event-only metadata as parser damage
- assuming that all overlay disappearance means the camera stopped sending metadata
- changing transport logic before the overlay-state meaning is explicit

### Current Interpretation
This case is not yet fully solved.

The main issue is that the runtime still determines overlay visibility through several distributed state signals rather than through one explicit overlay-state decision module.

That means the UI can show evidence, but it still has to infer too much.

### What Is Already Known
- raw metadata and parser-health categories can already be surfaced
- no-object metadata is a valid condition, not necessarily a parser failure
- fragmented payloads and recovered continuations are real live conditions
- current evidence panels improved observability, but they do not yet fully explain overlay disappearance on their own

### Current Working Hypothesis
This case will become easier to solve only after overlay-state reasoning is made explicit.

Recommended direction:
- introduce a dedicated overlay-state decision layer
- emit an explicit overlay reason such as `no-raw-metadata`, `metadata-without-objects`, `parsed-but-stale`, or `overlay-visible`
- let the Qt shell display that runtime decision instead of reconstructing it indirectly

### What To Check First
1. Was raw metadata seen recently?
2. Did the latest parsed summary contain objects?
3. Did the runtime clear overlay state because freshness expired?
4. Is the user looking at a metadata absence case, a parser outcome case, or a display-state case?

### Evidence Sources
- Qt Operator State
- Evidence panel (`raw`, `parsed`, `overlay`, `fresh`, `age`)
- Recent Metadata list
- `parsed_summary.log`
- `session.log`

### Representative Session Walkthrough
Representative metadata-present transition example:
- session: `output/session-20260328-151938/`
- target: `profile2`
- `parsed_summary.log`: starts with repeated `status=no-objects message="metadata-without-objects" objects=0`, then transitions immediately into `status=success message="clean-object-payload" ...`

Observed sequence:
- video reached first frame quickly
- metadata did not link on the first attempt and needed one startup retry
- once metadata started flowing, the first parsed summaries were `metadata-without-objects`
- immediately after that, clean object-bearing payloads appeared with `Human` and `Car` objects

Why this example matters:
- this is a concrete case where `metadata present` does not mean `overlay should already be visible`
- the initial `metadata-without-objects` phase is not parser damage; it is a valid no-object metadata condition
- the later transition to clean object payloads shows why the system must distinguish `metadata exists but no object block yet` from `overlay-state failure`
### Current Product Decision
Do not keep chasing overlay absence as one vague symptom.

The next implementation direction should be to make overlay-state reasoning explicit before trying to “fix overlay” in the abstract.

### Learning Note For SungHwan
This case teaches a different engineering habit:
- when one symptom can come from multiple layers, first make the system classify the reason explicitly
- only then decide whether the fix belongs in transport, parser, overlay-state handling, or UI wording

## Comparison Table
| Case | Main Layer | Core Question | Current Status |
| --- | --- | --- | --- |
| RTSP startup / first-frame failure | transport / session readiness | Did the session become usable quickly enough? | tracked, but currently non-blocking |
| metadata present but overlay missing | parser / overlay-state / UI interpretation | Why is the overlay absent right now? | still open and needs explicit runtime classification |

## Recommended Use
When a new runtime problem appears:
1. decide which of the two cases it resembles first
2. collect evidence at that layer before changing architecture assumptions
3. update this document if the current interpretation changes materially

## Next Expansion
Future versions of this document can add:
- one real saved-session walkthrough for each case
- a timeline diagram for state transitions
- a Korean companion document if the failure-study format becomes a recurring learning asset

## References
- `docs/projects/CV++/ARCHITECTURE.md`
- `docs/projects/CV++/TASKS.md`
- `docs/projects/CV++/VERIFICATION_GUIDE.md`
- `docs/projects/CV++/ONVIF_METADATA_PIPELINE_INVESTIGATION.md`
- `docs/projects/CV++/MESSAGE_INVESTIGATION.md`
- `docs/projects/CV++/CPP_LEARNING_GUIDE.md`


