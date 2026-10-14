# Non-DMX finish-line bench result record

Status: NOT RUN. Do not mark a pass from a build or simulation.

Follow [the fixed production and expected cue times](../../examples/productions/ALL_NODES_BENCH_TEST.md). Use the persisted all-nodes-bench-test.shdo production, rather than the narrower Studio RAM timeline path.

## Setup

Date/time and operator:
Source commit and installed Director / P4 / Comms / node versions:
Production ID and deployed file:
Audio and emergency WAV files present:
Node IDs, saved pixel counts, output routing and test-load wiring:
Serial logs / video / photographs:

## Normal run

- [ ] LOAD succeeds and remains stopped until deliberate RUN.
- [ ] RUN reaches COMPLETE at approximately 22 seconds.
- [ ] Every output matches the expected timeline; audio plays continuously from 10–14 seconds.
- [ ] Director Nodes page remains responsive and status agrees with P4.
- [ ] Lamp extinguishes and MOSFET pulse returns OFF.

Observed results and failures:

## Emergency repeat

Restart the same production and assert Emergency during the 10–14 second audio cue.

- [ ] P4 latches Emergency and interrupts the timeline.
- [ ] Audio Node attraction playback stops.
- [ ] P4 emergency WAV overrides ambience (or stays stopped if emergency assets are absent).
- [ ] Lamp local Emergency audio behaves as documented.
- [ ] P4, C3 Pixel, Audio and Emergency show pixels become bright white.
- [ ] MOSFET outputs immediately go OFF.
- [ ] Deliberate Director clear leaves show/audio stopped; no automatic resume.
- [ ] A fresh deliberate LOAD/RUN works after clear.

Observed results, interruption time and failures:

## Decision

PASS / FAIL / NOT RUN:
Outstanding failures:
Frozen working source commit, firmware set and production archive location:

Only freeze the working set after both passes. If a check fails, repair that path and repeat the same scenario. DMX and optional draft features are outside this acceptance.
