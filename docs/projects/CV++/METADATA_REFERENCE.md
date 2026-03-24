# CV++ Metadata Reference

## Document Control
- Version: `v0.1`
- Status: `Drafted`
- Created: `2026-03-24`
- Last Updated: `2026-03-24`
- Owner: `Software Engineer Agent`

## Purpose
This document explains the parsed metadata values currently surfaced by CV++ so SungHwan can interpret runtime output, recent metadata lines, and session logs consistently.

## Parsing Status Values
### `success`
Meaning:
- the payload contained object metadata and the parser recovered the expected object blocks cleanly

Typical interpretation:
- strong evidence that the camera sent usable object metadata
- good candidate for overlay and session metrics

### `no-objects`
Meaning:
- the payload was parseable, but no `<tt:Object>` block was found

Typical interpretation:
- the camera sent metadata, but not object-bearing metadata for that payload
- often corresponds to event-only or non-object metadata

### `malformed-payload`
Meaning:
- metadata arrived, but the XML payload was fragmented, incomplete, or otherwise not cleanly closed

Typical interpretation:
- this does **not** mean metadata was absent
- it usually means the parser had to recover objects from a partial payload
- still useful for overlay and evidence if object fields were recovered

Current observed frequency:
- this is currently the most common status in live sessions, so it should be treated as a normal runtime condition, not a rare exception

## Message Values Seen So Far
### `""` (empty message)
Meaning:
- no extra parser note was attached

Interpretation:
- low information value for UI
- usually safe to omit from operator-facing summaries

### `Object block did not close cleanly`
Meaning:
- an object block was cut off before the XML closed normally

Interpretation:
- important parser-health signal
- object fields may still be partially recovered
- useful in logs and parser-health metrics

### `XML start marker not found`
Meaning:
- the current payload chunk did not contain the XML start marker

Interpretation:
- usually indicates fragmented transport / chunk boundary issues
- useful for debugging payload assembly behavior

### `No <tt:Object> blocks found`
Meaning:
- the payload contained metadata but no object block

Interpretation:
- useful evidence when overlay is absent but metadata traffic still exists

### `Objects parsed successfully`
Meaning:
- the parser recovered object blocks cleanly

Interpretation:
- strong positive signal
- useful for success-rate metrics, but too repetitive for dense operator UI

## Object Fields
### `id`
Meaning:
- camera-reported object identifier (`ObjectId`)

Interpretation:
- use this to see whether the camera is treating repeated detections as the same tracked object
- repeated appearances of the same `id` are not new unique objects; they are repeated detection events for one camera-reported object

### `type`
Meaning:
- camera-reported object class after parser normalization

Most common values seen so far:
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
- `Car` and `Vehicle` can both appear in real sessions
- fragmented payloads can still produce damaged labels such as `Hu`, `Ca`, or `Caar`; these should be treated as parser-noise rather than stable classes

### `score`
Meaning:
- camera-reported likelihood / confidence value

Interpretation:
- useful for comparing repeated detections of the same object id over time
- should be read as a camera-produced score, not an app-generated confidence

## Practical Reading Guide
### When overlay is present
- the camera sent object metadata
- the app parsed enough fields to render it

### When overlay is absent but `raw=seen`
- inspect `parsed` count and recent metadata summary
- likely causes: event-only metadata, malformed payload with no recoverable object, or stale overlay state

### When `malformed-payload` is present
- do not assume failure immediately
- first check whether objects were still recovered
- if `objects > 0`, the payload was still operationally useful

### When the same object remains on screen
- compare `ObjectId` in overlay labels
- same `ObjectId` across frames suggests the camera is maintaining one track
- changing `ObjectId` for the same visible object may indicate track instability

## Current UI Guidance
Recommended operator-facing presentation:
- show `status` in a simplified way
- suppress empty `message` values
- retain important parser notes such as `Object block did not close cleanly`
- show `ObjectId` with overlay labels for track continuity inspection
- prefer compact summaries such as:
  - `malformed-payload | objects=2 | Car #165420 (57%), Vehicle #165454 (91%)`

## Next Reference Work
The next useful extension of this document would be:
- mapping raw XML fields to parsed runtime fields
- documenting which class labels come directly from camera metadata versus parser normalization
- adding real examples for `Car`, `Human`, `Bicycle`, `Vehicle`, and event-only payloads
