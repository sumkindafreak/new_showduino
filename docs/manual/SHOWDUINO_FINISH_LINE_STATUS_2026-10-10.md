# Showduino finish-line manual status — 10 October 2026

The existing commercial manual revision 0.4-RC describes the 7 October baseline. The newer, unmerged PR #29 combines PR #27 and PR #28, with P4 0.6.12, Comms 0.5.7 and Director 0.9.13-director source versions. These are not confirmed installed firmware versions.

PR #29 host and embedded Studio checks passed, but physical LOAD → RUN → COMPLETE acceptance has not been recorded. The fixed 22-second scenario is in `examples/productions/all-nodes-bench-test.shdo`; follow `examples/productions/ALL_NODES_BENCH_TEST.md`.

Ten pixels are the default for P4, C3 Pixel Node, Audio Node show pixels and Emergency Node show pixels, unless valid saved settings override them. Lamp Jewel remains seven pixels.

Still to verify physically: complete scenario playback, Audio Node continuity, Director ambience, Emergency Node white pixels, Lamp emergency sound, MOSFET output and Director Nodes-page stability. Re-run the scenario with Emergency asserted and confirm no automatic resume.

For the latest integration/CI/merge status, see `docs/pr27-pr28-integration-2026-10-10.md`. Do not treat the current `main` branch alone as the completed integrated firmware.

## Update: 11 October 2026

Main is now at `639555a` following two timestamp-only Studio bundle regenerations. Draft PR #29 remains unmerged and needs reconciliation with main. Its host checks pass, but the fixed 22-second scenario has not yet been physically signed off. PR #30 is separate optional Emergency Node hardware work and is outside this finish-line acceptance.


## 13 October 2026 review

Main is now `65a0872a` (13 October). Its latest change only refreshes the embedded Studio build timestamp; asset hash `b80ad3255289` is unchanged. PR #29 remains draft, unmerged and three commits behind main; its 12 October host and Studio checks passed. There is still no recorded physical 22-second LOAD-to-COMPLETE acceptance run. The separate draft Pixel Engine 2.0 work is outside this finish-line gate. Next: reconcile PR #29, flash matched firmware, run the fixed all-node scenario, repeat with Emergency mid-show, and record results before release.

## 14 October 2026 review — reconciled, builds passing, bench not yet run

The 13 October merge status above is historical. Current `main` is `c879c261` (14 October); its latest commits regenerate **timestamps only**, retaining embedded Studio asset hash `b80ad3255289`. Draft PR #29 has incorporated the latest `main` history without discarding the integrated firmware or 42-asset Studio bundle. GitHub currently reports PR #29 **mergeable**, with no commits behind `main`, but it remains **unmerged** and must not be represented as the installed or released firmware.

The 14 October reconciliation validation on commit `9959e60` passed all 16 host suite groups, Director/node/Studio regressions, production compile/persistent reload, the actual ambience parser, and embedded-asset validation. [Host checks](https://github.com/sumkindafreak/new_showduino/actions/runs/38610600889) and [embedded Studio checks](https://github.com/sumkindafreak/new_showduino/actions/runs/38610601071) passed. Fresh ESP32 Arduino core 3.3.11 target builds also passed: Director 1,652,862 bytes (52% flash), P4 1,171,464 bytes (89%), Comms 1,399,171 bytes (41%). These are **software results**, not evidence of physical cue delivery or audio quality.

**Documentation divergence:** the commercial manual on `main` is still **0.2-RC / 26 September** and describes older firmware/node capabilities. The integration branch carries the updated **0.4-RC** manual, current operator guidance, updated README and the fixed non-DMX acceptance procedure. Preserve these together when integrating PR #29; do not copy the newer manual onto `main` as if the firmware had already landed.

**Hardware acceptance: NOT RUN / no dated full pass recorded.** Follow [the fixed 22-second scenario](../../examples/productions/ALL_NODES_BENCH_TEST.md) and [bench result record](SHOWDUINO_BENCH_RESULT_RECORD.md). First prove physical LOAD → RUN → COMPLETE with all expected pixels, Audio Node WAV continuity, Lamp Jewel and MOSFET OUT1; then rerun with Emergency during the 10–14 s audio cue, verify white pixel overrides, audio/Emergency routing, safe outputs and no automatic restart after Director clear. Verify Director ambience controls and Nodes-page responsiveness during commissioning. Fix only observed failures. Leave unrelated draft features out of this finish-line gate.


## 15 October 2026 review — main moved; docs corrected on integration branch

At 21:21 UTC on 15 October, `main` reached `d560e8494f4aadd2b9c94b3473514b526901a21a`. Its latest commits regenerate the S3 Studio WebUI bundle; they do **not** merge the finish-line firmware. GitHub comparison at review time shows draft PR #29 **66 commits ahead and 4 behind `main`**, and GitHub reports it as not mergeable in its current state. The prior 14 October mergeable/no-behind statement is historical, not current. Reconcile only after reviewing the generated asset changes; preserve the validated firmware and canonical Studio bundle.

The latest PR #29 software evidence is still valuable: full Director/P4/Comms builds, host regression suites, and a 22-second non-DMX SHDO compile/reload roundtrip passed. **No dated physical `LOAD → RUN → COMPLETE` pass is recorded.** The strongest reported hardware observation remains a wireless Emergency Node assertion triggering audible Audio Node playback from roughly 7 m away; that does not validate the entire production. Audio WAV stutter, P4 PCM5102A ambience/Director controls, Emergency Node GPIO2 white override, Director Nodes-page responsiveness, Lamp audio and MOSFET OUT1 remain to be signed off on the real bench.

**Documentation corrections in this branch:** P4 `BoardConfig.h` identifies firmware `0.6.12`; Audio Node `BoardConfig.h` identifies `0.4.7` with **GPIO5 single status pixel** and **GPIO22 programmable show pixels**; MOSFET `BoardConfig.h` identifies `0.1.3`. Their firmware READMEs and the Audio Node guide have been corrected. These are *source versions on the unmerged branch*, not proof that hardware has been flashed. The 26 September 0.2-RC manual on `main` must not be silently relabelled as current.

**Smallest next gate:** reconcile PR #29 with current `main` without expanding scope; flash a matched Director/Comms/P4 and required node set; deploy the existing [`all-nodes-bench-test.shdo`](../../examples/productions/all-nodes-bench-test.shdo); record physical `LOAD → RUN → COMPLETE` and cue observations in [`SHOWDUINO_BENCH_RESULT_RECORD.md`](SHOWDUINO_BENCH_RESULT_RECORD.md); repeat with Emergency asserted mid-show, verify safe states and no auto-resume after Director clear. Correct only observed failures. Draft PR #30's separate two-pixel indicator and PR #31's FastLED expansion are **not** prerequisites.
