# Showduino software gap closure — 7 October 2026

Reviewed parent: `6e73003e6aad573c08ac8f5742d202b80b1ee895` on `docs/testing-stage-review-2026-10-07` (PR #26). This follow-up implements the software work below and updates manual revision **0.4-RC**. It does not record physical acceptance or claim that firmware binaries have been built/flashed.

| Area | Implemented in this follow-up | Remaining boundary |
| --- | --- | --- |
| Director ambience control | Settings → Audio System: SD WAV filename browsing, Play/Loop/Stop, 25/75/100% volume presets, P4 status/errors; control lock follows P4 status | Director target build, touch/layout and radio acceptance |
| Browser ambience control | System Console Outputs: filename/Next file browsing, Play/Loop/Stop, 0–100% volume and status; Comms and P4 share path/command validation; status/filename replies and commands fit the unchanged 96-byte Director wire field | Target build, tunnel/SD/DAC bench acceptance; no persistent Studio ambience cue |
| Ambience playback | Mono duplicated to stereo; unwritten I²S tails retained across loop iterations; truncated chunks/data rejected; fatal I²S writes stop playback | Continuous real DMA/SD operation, simultaneous ES8311 and PCM5102A output |
| Emergency audio | Prior P4 emergency override retained, including 100% emergency software volume, fallback WAV, latched command lock and clear-to-idle without restore. Lamp remains configured to play its local emergency WAV | Hear/measure all participating hardware outputs, missing-asset cases and STOP/Clear behaviour |
| Persistent Emergency Node pixels | `ESTOP` cue type accepts only `ESTOP:NODE:PIXEL:`. Real SHDO compiler → generated JSON → P4 parser roundtrip preserves every generated cue | GAP-021 Studio inventory surfacing; actual mixed-device show/deploy acceptance |
| Generic digital I/O | Browser Outputs commissioning for lines 1/2: mode, active polarity, pull, debounce preset, On/Off/Pulse, status, All off and Save. Shared bounded policy rejects malformed commands. API marks `:ERROR:` replies rejected | Director editor; input-to-scene binding; persistent IO cues; external-interface/polarity/SD/stop acceptance |
| Regression checks | New host-check workflow covers the active Arduino source paths, shared protocols and live Outputs page | Host checks do not compile board firmware. Existing P4 build workflow targets the separate `stage-engine/esp32-p4` project |

## Versions

| Component | Source version after this follow-up |
| --- | --- |
| P4 Stage Engine | 0.6.10 |
| S3 Comms | 0.5.5 |
| Director | 0.9.12-director |
| Lamp | 0.4.3, unchanged |
| Product | 1.0.0-rc.1, unchanged |

Release binaries, OTA manifests and installed devices have not been updated by changing these source constants.

## Verification performed

- `bash tools/ambience-tests/run_tests.sh`: compiles the actual `StageAmbience.cpp` with SD/I²S fixtures; tests emergency override/fallback/lock/clear, mono output, zero/partial writes, timeout/fatal write failures, truncated WAV rejection, filename browsing and command policies.
- `bash tools/production-tests/run_tests.sh`: passed 189 assertions across format/store/runtime/web policy/version/SHDO/framing checks, including compiled ESTOP pixel persistence. Existing compiler warnings in `showduino_shdo.h` remain.
- `node tools/studio-v4-tests/test_outputs_controls.js`: executes the actual Outputs page with DOM/transport fixtures; tests filename selection, commands, path traversal rejection, I/O pulse/all-off, Emergency and offline locks.
- `node tools/studio-v4-tests/test_pixel_authoring.js`: existing 17 authoring checks pass.
- JavaScript syntax and `git diff --check`: pass.
- Address/undefined-behaviour sanitizer check passed with leak detection disabled; LeakSanitizer cannot inspect processes in this execution sandbox.
- S3 flash assets regenerated with the current commissioning UI. The 39 existing authoring Studio assets remain byte-for-byte unchanged at pinned website commit `8a53b1665eec19dc2230b68e98d315b9e3b45b33`; no website authoring changes are claimed here.

The environment has no Arduino target toolchain. Director LVGL/radio execution, P4 and Comms target builds, binary releases and hardware acceptance are **not verified**. The added workflow supplies reproducible host checks; its remote run result must be read separately.

## Most important remaining work

1. Build the three changed board targets and run a real Director/browser → Comms → P4 ambience test, including muted normal ambience, Emergency WAV override, normal STOP and clear-to-idle. Confirm Lamp emergency playback too.
2. Record Emergency/latch/power-loss/clear acceptance, plus mixed-output show STOP and radio-disconnect behaviour. Hardware gaps are still open.
3. Surface Emergency Node pixel outputs consistently in Studio inventory (GAP-021) and accept a persisted mixed-device production on hardware (GAP-007).
4. Define and implement input-to-scene binding and persistent IO/ambience authoring. Browser commissioning does not provide those features.
5. Reproduce any Director Nodes-page freeze on the device with a log/traffic trace. No specific freeze defect was established or claimed fixed by this source review.

See the [updated gap report](manual/SHOWDUINO_MANUAL_GAPS.md), [user manual](manual/SHOWDUINO_USER_MANUAL.md), [Director guide](manual/SHOWDUINO_DIRECTOR_OPERATOR_GUIDE.md) and [source matrix](manual/SHOWDUINO_MANUAL_SOURCE_MATRIX.md).
