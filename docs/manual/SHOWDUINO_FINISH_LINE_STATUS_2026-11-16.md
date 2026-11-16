# Showduino finish-line evidence — 16 November 2026

## GitHub main comparison
Compared 14 November main baseline `f907670f` to `main`: four commits ahead, zero behind. Only `firmware/s3-comms-controller/ShowduinoS3CommsController/src/web/WebAssets.generated.h` and `tools/embed-webui/last-build.json` changed. This is generated S3 Studio content, not a P4/Director/node firmware implementation change. It is not hardware acceptance.

## Confirmed versus reported
- First coordinated audio and visual playback: owner confirmed 29 October 2026.
- Local 22-second full-node production: owner reports running, not formally accepted as two consecutive P4-confirmed complete runs.
- Audio WAV playback, P4 pixels and Audio Node pixels: previously demonstrated.
- Local Emergency clear correction: owner reports fixed; full safe recovery not formally accepted.
- Exact flashed binary versions and end-to-end serial traces: not recorded in this repository review.

## Acceptance gaps / regressions
- Two repeatable LOAD → START → expected node cues → P4 COMPLETE runs are outstanding.
- Emergency assert during show, safe output override, physical release, authorised Director clear and no automatic restart require a physical test with hazardous effect loads disconnected.
- Director reset / Cue Library navigation and MOSFET reconnect output drop need retest only if they interfere with the existing acceptance show.
- Website Studio to P4 SHDO parsing (#63), segment numbering (#64) and embedded Studio parity (#66) remain candidate changes requiring isolated validation; do not introduce merely to replay an already compiled SD production.
- Preserve known-good flashed firmware and show assets; reconcile local bench with GitHub before integrating any candidate.

## Minimum next action
1. Preserve and identify installed Director, P4, Comms and node firmware and the exact SD/audio assets.
2. Record two complete runs of the existing 22-second show with P4 terminal state.
3. Test Emergency interruption, clearance and no auto-resume with hazardous loads disconnected.
4. Investigate only defects reproduced by those tests.
5. Merge only verified changes needed to finish this existing non-DMX scenario.

This is a documentation-only source and owner-report reconciliation, not a new bench test, firmware flash or release approval.
