# Showduino — non-DMX finish-line status (18 October 2026)

**Goal:** Physically run one complete, existing 22-second `all-nodes-bench-test.shdo` scenario (LOAD → RUN → COMPLETE), then interrupt the same scenario with Emergency and verify safe recovery. **Not a release sign-off.**

**Reviewed:** `main` at [`abfb500`](https://github.com/sumkindafreak/new_showduino/commit/abfb500904f0aeeb7d7122049fd52cd4ae9e4a28), 18 October 2026, 20:41 UTC. The most recent main commit regenerates S3 Studio assets. The fixed acceptance production and integrated fixes remain on [draft PR #29](https://github.com/sumkindafreak/new_showduino/pull/29), head `3d8a887` (15 October).

## Evidence ledger

| Item | Evidence | Acceptance |
| --- | --- | --- |
| Director, P4, Comms source builds | 14 October PR #29 record: all three compiled with Arduino-ESP32 3.3.11; Director 52% flash, P4 89%, Comms 41% | **Software pass only**, not proof these binaries are installed |
| Host regression and Studio bundle | 14 October PR #29 [host checks](https://github.com/sumkindafreak/new_showduino/actions/runs/38610600889) and [embedded Studio checks](https://github.com/sumkindafreak/new_showduino/actions/runs/38610601071) passed | **Software pass only** |
| Acceptance SHDO | PR #29 tests compile and persistently reload all commands/times; end cue at 22 seconds | **Software pass only** |
| Wireless Emergency audio | Owner previously observed Emergency Node triggering audible Audio Node playback from about 7 m | **Individual physical observation**; not a full-show or recovery pass |
| All-node LOAD → RUN → COMPLETE | No dated complete real-hardware run or signed bench result found | **NOT RUN / NOT RECORDED** |
| Mid-show Emergency repeat | No dated full real-hardware interruption/clear/restart result found | **NOT RUN / NOT RECORDED** |

Do not infer physical success from CI, simulator output, a Studio preview, or the previous wireless Emergency audio observation.

## Integration blocker and source-version split

As of this review, PR #29 is **open, draft, unmerged, and reported not mergeable**. GitHub comparison shows **68 commits ahead / 17 behind** `main`. It must be reconciled against current main before integration; inspect generated Studio header and manifest together, preserving tested firmware and assets. Do not force-merge or assume earlier mergeability reports remain current.

The `main` source declares P4 **0.6.8**, Comms **0.5.4**, Audio **0.4.3**. PR #29 declares P4 **0.6.12**, Comms **0.5.7**, Audio **0.4.7**. These are *source declarations*, **not confirmed installed hardware versions**. Record the actual flashed versions during commissioning.

**Important wiring difference:** on current `main`, Audio GPIO22 is described as a status pixel. On PR #29, Audio **GPIO5 is one status pixel** and **GPIO22 is the separate programmable show strip**. PR #29's 22-second scenario expects **10 show pixels on GPIO22**. Never use a `main` wiring description to flash/wire the PR #29 Audio firmware without checking its `BoardConfig.h`. Likewise, PR #29 uses a ten-pixel default for the P4 GPIO23, C3 Pixel, Audio show and Emergency show lines, unless valid saved commissioning counts override it; the Lamp's Jewel remains fixed at seven.

## Exact acceptance production (on PR #29, not main)

Source: [`all-nodes-bench-test.shdo`](https://github.com/sumkindafreak/new_showduino/blob/integration/pr27-pr28-current-main/examples/productions/all-nodes-bench-test.shdo); [instructions](https://github.com/sumkindafreak/new_showduino/blob/integration/pr27-pr28-current-main/examples/productions/ALL_NODES_BENCH_TEST.md); [result record](https://github.com/sumkindafreak/new_showduino/blob/integration/pr27-pr28-current-main/docs/manual/SHOWDUINO_BENCH_RESULT_RECORD.md).

Before attempting a live bench test, verify matching flashed firmware and node IDs, saved counts, P4/Audio SD cards and `/showduino/audio/system-test.wav`; use a **benign low-voltage test load only** on MOSFET OUT1. The P4 ambience control test separately expects `/showduino/audio/ambience.wav`. Keep mains-powered props and other hazardous loads isolated.

| Time | Expected physical action |
| ---: | --- |
| 1–3 s | P4 GPIO23: ten blue BREATHE pixels, then black |
| 4–6 s | C3 `LED-01`: ten red FLICKER pixels, then black |
| 7–9 s | Audio GPIO22: ten FIRE pixels, then black |
| 10–14 s | Audio Node `system-test.wav` at volume 50, then STOP; **no stutter** |
| 15–18 s | Lamp seven-pixel Jewel: IGNITE → UNSTABLE → FLARE → EXTINGUISH |
| 19 s | `MOSFET-01` OUT1: 25% pulse for 500 ms, then OFF |
| 22 s | P4 COMPLETE log cue; Director reflects confirmed show state |

First run to COMPLETE. Also verify Director Nodes-page responsiveness and, separately, P4 PCM5102A ambience STATUS/PLAY/LOOP/VOL/STOP from the Director.

Then restart the **same** production and assert Emergency during the 10–14 s audio cue. Verify P4 latch and timeline interruption; Audio Node attraction audio STOP; P4 ambience Emergency WAV override (or stopped if required asset absent); Lamp local Emergency audio; P4/C3/Audio/Emergency pixel lines bright white; MOSFET outputs OFF; and Director-confirmed clear **does not automatically resume** show or audio. Require a fresh deliberate start after clearance. Do not treat Showduino Emergency as a certified life-safety E-stop.

## Observed or suspected regressions — physical status

The previous bench discussions identified **Audio Node stuttering**, **Emergency Node GPIO2 pixels not responding**, possible **Director Nodes-page freezing**, and unverified **Lamp Emergency audio**, **PCM5102A ambience controls**, and **MOSFET OUT1**. These are **not newly confirmed failures** on the current integration: they are acceptance checks to retest, with exact logs and installed firmware recorded if they reproduce.

Separate draft [PR #33](https://github.com/sumkindafreak/new_showduino/pull/33) addresses Emergency latch persistence but explicitly requires physical/security review and says **DO NOT MERGE**. It is not evidence that persistence has passed. Draft PR #30's extra status LEDs and PR #31's FastLED expansion are not prerequisites for this non-DMX scenario.

## Documentation synchronization

The commercial [User Manual on main](SHOWDUINO_USER_MANUAL.md) remains **0.2-RC**, based on 26 September `8c49ebd`. PR #29 contains an updated **0.4-RC** manual, Quick Start, operator guide, production documentation and the acceptance fixture. Do **not** relabel the older manual as though unmerged firmware is already on `main`. This dated status addendum explains the difference; incorporate the newer manual **with** the accepted firmware when the integrated hardware test passes.

**Smallest next steps:** (1) reconcile PR #29 without changing unrelated features; (2) flash/confirm a matched source set and SD assets; (3) run and record one real LOAD → RUN → COMPLETE; (4) rerun with Emergency during the audio cue and record clear/no-auto-resume; (5) fix only observed blockers, repeat, and then archive the accepted versions/results. **No DMX scope.**
