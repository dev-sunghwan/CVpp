# CV++ Message Investigation

## Document Control
- Version: `v0.3`
- Status: `Updated to current parser taxonomy`
- Created: `2026-03-24`
- Last Updated: `2026-03-25`
- Owner: `Software Engineer Agent`

## Purpose
This document records what the parser `message` values appear to mean based on real saved sessions and the current parser implementation, not only parser intent.

## Scope
Investigated message cases:
- historical labels from saved sessions before `2026-03-24`
- current labels from the updated parser
- how fragmented Hanwha payloads move through truncation and continuation states

## Finding 1: fragmented object delivery is still the dominant baseline
Aggregate parsed summary distribution across saved sessions:
- `malformed-payload`: `21288`
- `no-objects`: `3492`
- `success`: `4425`

When restricted to payloads with `objects > 0`:
- `malformed-payload`: `17721`
- `success`: `4425`

Object-bearing payload ratios:
- `malformed-payload`: `80.02%`
- `success`: `19.98%`

Interpretation:
- object-bearing metadata is common, but clean object-bearing payloads are still the minority baseline
- parser-health messaging remains operationally important for Milestone 3

## Finding 2: the newer taxonomy is a relabeling of older observed behavior
Historical-to-current mapping:
- `Object block did not close cleanly`
  - current intent: `truncated-object-fragment`
- empty message on `malformed-payload`
  - current intent: continuation recovery that is now labeled `recovered-continuation`
- `No <tt:Object> blocks found`
  - current intent: `metadata-without-objects`
- `XML start marker not found`
  - current intent: `continuation-without-xml-start`

Interpretation:
- the new labels are better aligned with stream behavior
- they do not imply a different camera condition than older saved sessions

## Finding 3: `truncated-object-fragment` is the correct meaning for the older truncation note
Observed behavior:
- the current parser emits `truncated-object-fragment` when an object block starts but does not close in the current payload
- this matches the older `Object block did not close cleanly` behavior seen in `session-20260323-133443`

Representative current example:
- session: `session-20260324-163207`
- parsed line:
  - `status=malformed-payload message="truncated-object-fragment" objects=2 id=169384,type=Head,... id=169375,type=Human,...`

Interpretation:
- this is the right operator-facing name for a usable but incomplete object payload

## Finding 4: `recovered-continuation` is the current continuation baseline
Observed behavior in the recent `profile2` baseline session `session-20260324-163207`:
- `recovered-continuation`: `2574`
- `truncated-object-fragment`: `2279`
- `clean-object-payload`: `148`
- `metadata-without-objects`: `4`

Representative sequence:
1. `status=malformed-payload message="truncated-object-fragment" objects=2 ...`
2. `status=malformed-payload message="recovered-continuation" objects=5 ...`
3. `status=success message="recovered-continuation" objects=9 ...`

Interpretation:
- continuation recovery is now an ordinary path, not a corner case
- the same continuation chain can be useful before it becomes fully clean

## Finding 5: `recovered-continuation` can end in either `malformed-payload` or `success`
Observed behavior:
- some continuation chains still end mid-object and remain `malformed-payload`
- other continuation chains eventually close cleanly and end as `success`

Interpretation:
- the `message` is describing the recovery path
- the `status` is describing whether the current parse ended cleanly or still ended fragmented
- UI should show both, not only one field

## Finding 6: `metadata-without-objects` remains a valid metadata condition, not parser damage
Observed behavior:
- current parser emits `status=no-objects message="metadata-without-objects" objects=0`
- earlier raw samples and saved sessions still show this aligns with event-only metadata such as motion and state events

Interpretation:
- no-object metadata should remain separate from malformed transport cases
- missing overlay with `raw=seen` should not immediately be blamed on parser failure

## Finding 7: parser-noise labels are real but rare
Aggregate detection baseline:
- total detections analyzed: `50856`
- noise-labeled detections outside the stable class set: `127`
- noise share: `0.25%`

Most common noise labels:
- `Cayr`
- `Hukuman`
- `Caar`

Interpretation:
- forensic tools should preserve these labels
- operator metrics should not elevate them to first-class categories

## PM / Tech Lead / SE Interpretation
PM view:
- operator surfaces should answer metadata-condition questions, not expose raw parser internals without context

Tech Lead view:
- the new labels are closer to stream lifecycle states and should become the runtime taxonomy baseline

SE view:
- raw parser notes should remain in logs
- Qt and later SQLite review should add a smaller parser-health breakdown built on top of these messages

## Working Interpretation Model
Current practical model:
- `success + clean-object-payload`
  - clean object payload
- `success + recovered-continuation`
  - continuation recovered and ended cleanly
- `unknown-pattern + clean-object-payload-with-unknown-patterns`
  - object payload present, but parser coverage is incomplete
- `no-objects + metadata-without-objects`
  - metadata without object blocks
- `malformed-payload + truncated-object-fragment`
  - object payload cut mid-fragment
- `malformed-payload + continuation-without-xml-start`
  - continuation chunk without XML start
- `malformed-payload + recovered-continuation`
  - continuation recovery still useful, but current parse still ended fragmented

## Message Taxonomy v2
Recommended separation:
1. Keep raw parser `status` and `message` in logs and forensic views.
2. Add a smaller operator-facing parser-health category layer in the UI and later SQLite review.

Recommended operator-facing categories:
- `clean object payload`
- `recovered continuation`
- `fragmented object payload`
- `continuation chunk`
- `metadata without objects`
- `unknown object pattern`

Why this is better:
- removes empty-message ambiguity from older sessions
- fits the current parser implementation directly
- reflects real stream behavior seen in Hanwha sessions

## Recommendation
Treat the current parser message set as a useful forensic layer, and build the operator-facing parser-health view from it rather than replacing it outright.

Recommended next step:
- keep the raw `status` and `message` fields in logs
- add grouped parser-health counts to the Qt shell and later SQLite session review
- keep historical old-message interpretation documented for comparison with older saved sessions

## Open Questions
- whether `recovered-continuation` should later be split into `recovered-clean` and `recovered-still-fragmented` operator categories
- whether the parser should persist an explicit continuation flag instead of deriving it from `message`
- whether parser-noise labels should be grouped as `parser-artifact` in session metrics or only in UI summaries
