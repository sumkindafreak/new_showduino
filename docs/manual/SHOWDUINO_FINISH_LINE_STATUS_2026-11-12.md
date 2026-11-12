# Showduino finish-line evidence — 12 November 2026

> Documentation-only snapshot. No firmware flashed, no hardware acceptance implied. Goal: one complete repeatable non-DMX scenario.

## Source comparison
Compared GitHub `main` with commit `8dafd877` (11 November). Two additional commits modify only `firmware/s3-comms-controller/ShowduinoS3CommsController/src/web/WebAssets.generated.h` and `tools/embed-webui/last-build.json`. These are generated embedded Studio assets, **not** proof that P4 firmware or nodes changed, that the live Studio website is synchronized, or that hardware was tested.

## Latest bench evidence
- 29 October: first coordinated physical audio/visual playback confirmed.
- Existing 22-second all-node scenario reported running locally, but no signed two-run LOAD → START → COMPLETE record.
- Local Emergency clear reportedly repaired after the 31 October AWAITING STAGE regression; complete Emergency interruption, authorised clearance and no-auto-resume acceptance remains outstanding.
- Director navigation/reset stability and MOSFET output retention during Director reconnect need targeted retesting.

## Source-level gaps and documentation corrections
- The main User Manual begins with a **4 November** snapshot and a **20 October** firmware/manual baseline; it is not a current operational release sign-off. Read this dated supplement alongside it. Do not silently update the manual's historical firmware baseline.
- [PR #63](https://github.com/sumkindafreak/new_showduino/pull/63) documents that current main's SHDO compiler rejects website Studio exports; the candidate fixes parser consumption, stable cue order, truncation refusal and safety-flag validation. Host checks are not physical acceptance.
- [PR #64](https://github.com/sumkindafreak/new_showduino/pull/64) aligns pixel segment allocation between P4 compilation and website Studio; newly compiled exports can change their segment assignments, while persisted SD productions are unaffected.
- [PR #66](https://github.com/sumkindafreak/new_showduino/pull/66) aligns embedded Communications S3 Studio segment allocation and RAM-upload preflight with website Studio; it is stacked on #64/#63 and not a released main feature.
- [PR #65](https://github.com/sumkindafreak/new_showduino/pull/65) records simplified C3 Lamp integration as **specification only**, not implemented/accepted firmware. Do not make this new node a prerequisite for the existing scenario.
- [PR #51](https://github.com/sumkindafreak/new_showduino/pull/51) preserves a candidate local firmware baseline. Verify against actually flashed files before integration. [PR #58](https://github.com/sumkindafreak/new_showduino/pull/58) is a candidate MOSFET reconnect correction, not hardware-proven.

## Minimum acceptance path
1. Freeze/copy exact working flashed binaries/source, SD show, audio and node configurations.
2. Twice run the existing 22-second scenario: LOAD, START, every expected cue and P4-confirmed COMPLETE; record serial and observed output results.
3. With hazardous loads disconnected, interrupt with physical Emergency, verify safe output state, release physical input, clear only via authorised Director/P4 sequence, confirm show stays stopped.
4. Retest Director navigation/reconnect and MOSFET outputs only if they affect this scenario.
5. If freshly exported website Studio SHDO is needed, validate #63 on isolated bench before #64/#66. Otherwise defer these integrations and use the known-good persisted production.

No DMX, FastLED conversion or additional Lamp feature is required for this finish line.
