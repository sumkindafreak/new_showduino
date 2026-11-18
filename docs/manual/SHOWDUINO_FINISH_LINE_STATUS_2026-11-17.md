# Showduino finish-line evidence — 17 November 2026

## Scope and evidence
Goal: one complete, repeatable, safely controlled non-DMX scenario on existing hardware. GitHub `main` compared with 14 November commit `f907670f`: six commits ahead; net changed files only `firmware/s3-comms-controller/ShowduinoS3CommsController/src/web/WebAssets.generated.h` and `tools/embed-webui/last-build.json`. This is a *net diff*, not a claim that every intermediate commit was inspected independently. No active P4/Director/node source difference in this comparison. Generated bundle updates are not bench acceptance.

## Last reported physical results
- 29 October: first combined audio and visual playback on physical hardware.
- Existing 22-second show: owner-reported running locally.
- Audio playback, P4 and Audio Node pixels: previously demonstrated.
- Local Emergency clear: owner-reported fixed, but full safe recovery not formally accepted.
- No recorded two consecutive P4-confirmed LOAD → START → COMPLETE runs.
- No recorded Emergency assert → safe outputs → button release → authorised Director clear → no auto-resume pass.
- Director resets/navigation defects and MOSFET reconnect output drop need targeted hardware retest when relevant.

## Source-versus-manual discrepancies
The User Manual retains its 20 October source-reviewed firmware baseline; it must not be read as a current flashed-build certification. The 17 November addendum clarifies current evidence. Website Studio SHDO compiler parsing (#63), pixel segment numbering (#64), and embedded S3 Studio parity (#66) are unmerged candidates, not accepted installed features. #63 documents website Studio exports rejected by main; #64 documents different segment allocation on compile versus live upload; #66 addresses embedded Studio parity. These matter only if the finish-line show requires new Studio exports. Already persisted SD productions need not be recompiled for this test. Do not merge the isolated candidates or replace working flashed firmware without an isolated bench test.

## Minimum acceptance sequence
1. Back up exact flashed Director, P4, Comms and Node binaries/config, the known-good SD production, and media.
2. With hazardous effects disconnected, run the existing 22-second production twice; log P4 LOAD, START, cue actions, COMPLETE and expected audio/pixel/MOSFET output.
3. Interrupt a separate run via physical Emergency; verify protected outputs, physical release and Director-authorised clear, and verify no automatic restart.
4. Reproduce Director instability or MOSFET reconnect only if they affect the acceptance fixture; integrate only a proven essential correction.
5. Compare working local firmware to GitHub before any merge or flash. No DMX, optional FastLED, or new Lamp scope.

## Acceptance state
First coordinated playback achieved; repeatable scenario and full Emergency recovery acceptance outstanding. No hardware was accessed, firmware flashed, or acceptance newly established by this documentation update.
