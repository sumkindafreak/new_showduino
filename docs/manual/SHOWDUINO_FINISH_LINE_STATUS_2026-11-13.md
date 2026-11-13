# Showduino finish-line reconciliation — 13 November 2026

**Goal:** One complete, repeatable, safely controlled non-DMX scenario on the existing bench. **Not a release or hardware acceptance.**

## Repository evidence

Compared GitHub `main` against `8dafd877185708c87990a546348082680070d416` (11 November): **five commits ahead, zero behind**. Only two paths differ:
- `firmware/s3-comms-controller/ShowduinoS3CommsController/src/web/WebAssets.generated.h`
- `tools/embed-webui/last-build.json`

These are generated embedded Studio assets/manifest. This comparison establishes **no active P4/node firmware implementation change**, nor a hardware test. The exact latest `main` SHA and each commit's timestamp were not independently retrieved in this review; do not infer them from the comparison.

## Latest bench evidence (owner-reported, not new tests today)

- 29 October: first physical combined audio-and-visual playback achieved.
- Existing 22-second all-node bench show reported running locally.
- Audio Node playback, P4 pixel line and Audio Node pixels previously demonstrated.
- Local Emergency-clear regression reported fixed after 31 October failure. Do **not** replace the flashed working implementation with older `main` or an unvalidated PR.
- No logged, signed evidence of **two consecutive LOAD → START → expected cues → P4 COMPLETE** runs, Emergency interruption → safe outputs → released input → Director-authorised clear → no automatic restart.
- Director reset/navigation regressions and MOSFET output loss on Director reconnect need targeted repeat checks where they affect this scenario.

## Documentation and source parity findings

The User Manual retains a historical **20 October** source-reviewed firmware baseline and a **4 November** operational header. This is valid historical provenance but misleading as the latest operational status without a prominent dated supplement. The 13 November addendum now distinguishes historical source review from current bench evidence.

- [PR #63](https://github.com/sumkindafreak/new_showduino/pull/63): the `main` SHDO compiler has been reported to reject website Studio exports with `INVALID_JSON: invalid clip object`. Isolated candidate repairs parsing, stable cue ordering, command length refusal, and safety flags; host tests only. Also records an Emergency Node pixel cue loader mismatch. **Conditional blocker only if the finish-line scenario is freshly exported from Studio.**
- [PR #64](https://github.com/sumkindafreak/new_showduino/pull/64): Studio and P4 segment assignment differ for newly compiled Studio shows. Existing compiled SD productions are not changed; bench validation required before merging.
- [PR #66](https://github.com/sumkindafreak/new_showduino/pull/66): embedded Communications S3 Studio segment and RAM-upload preflight parity candidate, stacked on #64/#63. No hardware acceptance.
- [PR #51](https://github.com/sumkindafreak/new_showduino/pull/51): working local firmware reconciliation candidate; compare against exact flashed local files before integrating.
- [PR #58](https://github.com/sumkindafreak/new_showduino/pull/58): proposed MOSFET reconnect recovery, hardware result outstanding.
- [PR #65](https://github.com/sumkindafreak/new_showduino/pull/65): simplified C3 Lamp integration documentation, not accepted firmware and not required for this scenario.

**No firmware was changed or flashed in this documentation update.** Draft PR descriptions and host checks are not proof of merged or physically accepted behavior.

## Minimum acceptance path

1. Preserve known-good flashed firmware, node configurations, SD production and audio assets; record versions.
2. Run existing 22-second production twice with serial capture: LOAD, START, each expected audio/pixel/node action, P4-confirmed COMPLETE.
3. With hazardous outputs physically disconnected, assert Emergency during playback; confirm defined safe outputs, release the physical switch, clear through Director/P4 authorisation, and verify no automatic resume.
4. Retest Director stability and MOSFET reconnect only where necessary for this scenario.
5. Only if a freshly exported Studio `.shdo` is required, isolate and bench-test the minimal #63 parser fix; evaluate #64/#66 only if their segment/upload paths are exercised.

Do not introduce DMX, optional FastLED conversion, or new Lamp Node work to close this milestone.

**Verdict:** First coordinated show achieved; repeatability and Emergency recovery not formally accepted; documentation reconciled on a review branch pending merge.
