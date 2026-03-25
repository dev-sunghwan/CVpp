# CV++ Hanwha Metadata Baseline Analysis

## Document Control
- Version: `v0.1`
- Status: `Drafted`
- Created: `2026-03-25`
- Last Updated: `2026-03-25`
- Owner: `Software Engineer Agent`

## Purpose
This document establishes the current Hanwha metadata baseline from saved CV++ sessions so the team can separate real camera behavior from parser artifacts before extending Milestone 3 and Milestone 8.

## Dataset And Method
- dataset: saved `parsed_summary.log` files under `output/session-*`
- session range: `2026-03-18` through `2026-03-24`
- aggregate parsed payload count analyzed: `29205`
- aggregate detected objects analyzed: `50856`
- representative current-taxonomy session: `output/session-20260324-163207/` (`profile2`)
- representative older object-heavy session: `output/session-20260323-133443/` (`profile4`)

Important caveat:
- the parser message taxonomy changed on `2026-03-24`
- older sessions still use chunk-local notes such as `Object block did not close cleanly` and empty-message `malformed-payload`
- the most recent baseline session uses the newer stream-aware labels such as `truncated-object-fragment`, `recovered-continuation`, and `metadata-without-objects`

## Aggregate Baseline
Across all saved parsed summaries:
- `malformed-payload`: `21288` (`72.89%`)
- `no-objects`: `3492` (`11.96%`)
- `success`: `4425` (`15.15%`)

When restricted to object-bearing payloads:
- total object-bearing payloads: `22146`
- `malformed-payload`: `17721` (`80.02%`)
- `success`: `4425` (`19.98%`)

Interpretation:
- fragmented transport is still the dominant live condition
- object-bearing metadata should not be equated with clean XML delivery
- operator surfaces must show parser-health context beside overlay state

## Stable Class Baseline
Most common normalized object types across all saved sessions:
- `Car`: `29859`
- `Human`: `9519`
- `Vehicle`: `8215`
- `Bicycle`: `1694`
- `Head`: `632`
- `Bus`: `536`
- `Truck`: `112`
- `Unknown`: `85`
- `Motorcycle`: `77`

Unique object ID baseline across all saved sessions:
- `Car`: `353`
- `Vehicle`: `228`
- `Human`: `75`
- `Unknown`: `61`
- `Bicycle`: `31`
- `Bus`: `21`
- `Head`: `12`
- `Truck`: `9`
- `Motorcycle`: `7`

Interpretation:
- `Car`, `Human`, and `Vehicle` remain the main stable classes
- `Vehicle` and `Car` both appear in real sessions and should not be treated as the same class automatically
- `ObjectId` is good enough to distinguish repeated detections from unique tracked objects inside one session

## Parser-Noise Baseline
Counts outside the stable normalized class set:
- total noise-labeled detections: `127`
- noise share of all detections: `0.25%`

Most common noise labels:
- `Cayr`: `16`
- `Hukuman`: `10`
- `Caar`: `8`
- `Hkauuman`: `6`
- `Caur`: `6`

Interpretation:
- parser-noise labels are real enough to matter, but still rare compared with stable normalized labels
- session metrics and later SQLite review should keep raw evidence available, but should avoid treating these labels as first-class product categories

## Current-Taxonomy Baseline
Representative recent session: `output/session-20260324-163207/` (`profile2`)

Payload counts:
- total payloads: `5005`
- `malformed-payload`: `2574` (`51.43%`)
- `success`: `2427` (`48.49%`)
- `no-objects`: `4` (`0.08%`)

Object-bearing payload counts:
- `malformed-payload`: `2574` (`51.47%`)
- `success`: `2427` (`48.53%`)

Message counts:
- `recovered-continuation`: `2574`
- `truncated-object-fragment`: `2279`
- `clean-object-payload`: `148`
- `metadata-without-objects`: `4`

Representative live sequence:
- `status=malformed-payload message="truncated-object-fragment" objects=2 ...`
- `status=malformed-payload message="recovered-continuation" objects=5 ...`
- `status=success message="recovered-continuation" objects=9 ...`

Interpretation:
- continuation handling is now part of the normal baseline, not a rare exception path
- `recovered-continuation` can end in either `malformed-payload` or `success`
- the parser is often operationally useful before the payload is fully clean

## Historical Comparison Baseline
Representative earlier session: `output/session-20260323-133443/` (`profile4`)

Payload counts:
- `malformed-payload`: `3152`
- `no-objects`: `542`
- `success`: `0`

Older message-note counts:
- `Object block did not close cleanly`: `1647`
- empty message on `malformed-payload`: `1505`
- `No <tt:Object> blocks found`: `542`

Interpretation:
- the newer taxonomy is not inventing a new stream condition
- it is a clearer relabeling of the same fragmentation and continuation behavior that older sessions already showed

## Practical Baseline Conclusions
- Hanwha metadata is present and object-bearing on both `profile2` and `profile4`
- fragmented delivery remains the default operating condition, so UI and storage should model it directly
- `ObjectId` is already useful enough for unique-object metrics and later review workflows
- parser-noise labels should stay visible in forensic views, but should be grouped or down-weighted in operator summaries
- plain logs remain the raw evidence source of truth

## Milestone Impact
Milestone 3 should continue with:
- explicit parser-health breakdown in the Qt shell and session summary
- a visible distinction between clean payloads, recovered continuations, fragmented object payloads, and metadata-without-objects
- suppression or grouping of parser-noise labels in headline metrics

Milestone 8 should continue with:
- SQLite tables that preserve session summaries, parsed payload status, and object detections
- raw XML remaining in `metadata_raw.xml.log` instead of moving wholesale into SQLite

## References
- `output/session-20260324-163207/parsed_summary.log`
- `output/session-20260324-163207/session.log`
- `output/session-20260323-133443/parsed_summary.log`
- `output/session-20260323-133443/session.log`
- `docs/projects/CV++/METADATA_REFERENCE.md`
- `docs/projects/CV++/MESSAGE_INVESTIGATION.md`
