# Showduino finish-line status — 16 October 2026

**Goal:** One physical non-DMX scenario reaching COMPLETE, then an Emergency interruption and safe recovery repeat. **No complete hardware pass is recorded.** This is a bench milestone, not production safety sign-off.

## Source of truth

- `main`: [0277d46](https://github.com/sumkindafreak/new_showduino/commit/0277d46b35d47c94812ae6ef862270cb17562fdb) (16 October, 21:06 UTC). Latest commit regenerates the S3 Studio bundle; it does not integrate the pending finish-line firmware.
- [PR #29](https://github.com/sumkindafreak/new_showduino/pull/29): draft, open, unmerged; at review time 68 commits ahead and 9 behind `main`. Its host tests, Director/P4/Comms builds and persistent 22-second production compile/reload passed. These are **software results**.
- [PR #33](https://github.com/sumkindafreak/new_showduino/pull/33): separate draft Emergency latch/station-blocker persistence work. It reports P4 CI and host tests passing but no hardware timing/behaviour verification. Its own review identifies wireless report authentication and service-recovery gaps. **Do not merge as release-ready.**
- Builder-reported partial physical success: a wireless Emergency assertion triggered audible Audio Node playback at about seven metres. This is not a complete show pass.

## Outstanding physical checks

Audio Node WAV playback without stutter; P4 PCM5102A ambience and Director controls; Emergency Node GPIO2 pixels turning white; Lamp emergency audio; Director Nodes-page responsiveness; MOSFET OUT1 pulse with a benign low-voltage load; full radio stability and all scheduled cue acknowledgements.

## Minimum path

1. Reconcile PR #29 with `main` while preserving tested firmware and Studio assets. Repeat build, host and production round-trip checks.
2. Flash matched Director/Comms/P4 and required specialist nodes; record versions, IDs, pixel counts and WAV paths.
3. Deploy and explicitly LOAD/RUN the [fixed 22-second acceptance production](https://github.com/sumkindafreak/new_showduino/blob/integration/pr27-pr28-current-main/examples/productions/ALL_NODES_BENCH_TEST.md). Observe each cue and COMPLETE.
4. Repeat during Audio Node playback with wireless Emergency asserted. Confirm attraction audio stops, emergency audio policy, white pixel overrides, MOSFET OFF, deliberate Director clear and **no automatic resume**.
5. Enter dated results in the [bench record](https://github.com/sumkindafreak/new_showduino/blob/integration/pr27-pr28-current-main/docs/manual/SHOWDUINO_BENCH_RESULT_RECORD.md); fix only failures and repeat.

## Manual reconciliation

The [main user manual](SHOWDUINO_USER_MANUAL.md), [source matrix](SHOWDUINO_MANUAL_SOURCE_MATRIX.md) and [gaps](SHOWDUINO_MANUAL_GAPS.md) remain at **0.2-RC / 26 September**. Do not treat unmerged PR #29 functionality as current main or installed hardware. Emergency latch power-loss persistence remains an open main-branch gap.

The [parts register](SHOWDUINO_BUILD_AND_PARTS_REGISTER.md) describes the three-board enclosure and replaceable nodes. **Audio Node wiring differs by branch:** main uses GPIO22 for its pixel implementation; PR #29 separates GPIO5 single status pixel and GPIO22 programmable show line. Check the actual flashed `BoardConfig.h` before wiring. P4 source labels are 0.6.8 on main and 0.6.12 on PR #29, not verified installed versions.

No DMX, FastLED migration or new effects are needed for this acceptance. Separate Emergency safety verification is still required before release.
