# CV++ Metadata Reference

## Document Control
- Version: `v0.2`
- Status: `Updated to current parser taxonomy`
- Created: `2026-03-24`
- Last Updated: `2026-03-25`
- Owner: `Software Engineer Agent`

## Purpose
This document explains the parsed metadata values currently surfaced by CV++ so SungHwan can interpret runtime output, recent metadata lines, and session logs consistently.

## Parsing Status Values
### `success`
Meaning:
- the payload ended in a clean enough state for the parser to treat it as a successful object-bearing parse

Typical interpretation:
- strong evidence that the camera sent usable object metadata
- can still appear with `message="recovered-continuation"` when a previously fragmented payload becomes complete

### `unknown-pattern`
Meaning:
- the parser found object blocks but at least one object detail pattern did not match the expected forms cleanly

Typical interpretation:
- object metadata is present
- the payload is useful, but parser coverage is incomplete for some object detail

### `no-objects`
Meaning:
- the payload was metadata, but no `<tt:Object>` block was found

Typical interpretation:
- usually event-only or non-object metadata
- useful evidence that metadata traffic exists even when overlay objects do not

### `malformed-payload`
Meaning:
- metadata arrived, but the current payload or continuation chain is still fragmented or incomplete

Typical interpretation:
- this does **not** mean metadata was absent
- objects may still be partially or mostly recoverable
- this remains a normal live condition in saved Hanwha sessions, not a rare exception

## Message Values Seen So Far
### `clean-object-payload`
Meaning:
- the payload contained object metadata and closed cleanly in the current chunk

Interpretation:
- strongest clean baseline
- useful for parser-health metrics

### `clean-object-payload-with-unknown-patterns`
Meaning:
- the payload closed cleanly, but some object-detail pattern was only partially understood

Interpretation:
- still object-bearing and useful
- indicates a parser-coverage gap rather than transport loss

### `metadata-without-objects`
Meaning:
- metadata was present, but no `<tt:Object>` block was found

Interpretation:
- usually event-only metadata
- should not be grouped with parser damage

### `truncated-object-fragment`
Meaning:
- an object block started, but the payload stopped before the object block closed

Interpretation:
- direct signal that transport fragmentation cut the object payload
- often followed by one or more continuation parses

### `continuation-without-xml-start`
Meaning:
- the current chunk did not contain `<?xml`, so it looks like a continuation fragment rather than a fresh XML start

Interpretation:
- useful transport-fragmentation signal
- usually means the parser is seeing a mid-stream continuation

### `recovered-continuation`
Meaning:
- the parser is working on a continuation chain and recovered usable object data from it

Interpretation:
- can appear with either `malformed-payload` or `success`
- if the continuation chain still ends mid-object, status remains `malformed-payload`
- if the continuation chain becomes complete, status can end as `success`

### `recovered-continuation-with-unknown-patterns`
Meaning:
- continuation recovery succeeded enough to produce objects, but some object-detail pattern still did not match the expected parser forms

Interpretation:
- transport recovery succeeded
- parser coverage still needs improvement for some detail path

## Object Fields
### `id`
Meaning:
- camera-reported object identifier (`ObjectId`)

Interpretation:
- use this to see whether the camera is treating repeated detections as one tracked object
- repeated appearances of the same `id` are repeated detection events, not new unique objects

### `type`
Meaning:
- object class after CV++ normalization

Most common stable values seen so far:
- `Car`
- `Human`
- `Vehicle`
- `Bicycle`
- `Head`
- `Bus`
- `Truck`
- `Motorcycle`
- `Unknown`

Notes:
- `Car` and `Vehicle` both appear in real Hanwha sessions
- saved-session analysis shows parser-noise labels exist, but they are rare relative to stable normalized labels
- labels such as `Cayr`, `Caar`, or `Hukuman` should be treated as parser-noise, not as stable product categories

### `score`
Meaning:
- camera-reported likelihood / confidence value

Interpretation:
- useful for comparing repeated detections of the same object id over time
- should be read as a camera-produced score, not an app-generated confidence

## Practical Reading Guide
### When overlay is present
- the camera sent object metadata
- the app parsed enough detail to render it

### When overlay is absent but `raw=seen`
- inspect `parsed`, `fresh`, and recent metadata summaries
- likely causes: event-only metadata, fragmented payload with no currently recoverable object, or stale overlay state

### When `malformed-payload` is present
- do not treat it as metadata absence
- first check `objects`
- if `objects > 0`, the payload was still operationally useful

### When `recovered-continuation` is present
- treat it as evidence that continuation handling is active
- if status is `success`, the continuation chain became complete
- if status is `malformed-payload`, the chain still ended mid-fragment

### When the same object remains on screen
- compare `ObjectId` in overlay labels
- the same `ObjectId` across frames suggests one continuing camera track
- changing `ObjectId` for the same visible object may indicate track instability

## Current UI Guidance
Recommended operator-facing presentation:
- show both `status` and a simplified parser-health category
- keep raw parser messages available in logs and forensic views
- group parser-noise labels separately from stable class metrics
- show `ObjectId` with overlay labels for track continuity inspection
- prefer compact summaries such as:
  - `success | recovered-continuation | objects=9 | Car #169173 (85%), Human #169375 (82%)`

## Next Reference Work
The next useful extension of this document would be:
- mapping raw XML fields to parsed runtime fields
- exposing parser-health category counts directly in the Qt shell
- documenting when a raw camera label becomes a normalized runtime label
