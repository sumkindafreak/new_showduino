# Showduino finish-line evidence reconciliation — 30 October 2026

## 10 November 2026 — source and bench evidence update

- **Verified `main`:** `efb2e13ddd50d837f9be85b6d2a46b91e6080781` (19:42:18 UTC). Three commits since `04152dc7551d91d510c2df6413e3d3cb4ba72ff1` (9 November 06:23 UTC), affecting only `firmware/s3-comms-controller/ShowduinoS3CommsController/src/web/WebAssets.generated.h` and `tools/embed-webui/last-build.json`. These are generated WebUI/manifest updates, not a merged working-firmware baseline or new bench evidence.
- **Latest owner bench evidence:** 29 October first combined audio/visual show; 2 November report of the 22-second full-node show running and Emergency clear corrected in flashed local firmware. No newer logged two-run P4 LOAD → START → COMPLETE or isolated mid-show Emergency assert/release/Director-authorised clear/no-auto-resume acceptance is available.
- **Risks:** Prior Director resets/Cue Library navigation issues need targeted retest. Reported MOSFET output drop on Director reconnect is not hardware-accepted as fixed; draft #58 proposes a correction and matching P4 guard. The local Emergency fix must be protected while comparing draft #51. FastLED drafts are optional and not a finish-line prerequisite.
- **Smallest non-DMX actions:** preserve flashed versions and show/SD assets; run and log the same complete show twice; test Emergency safely with hazardous loads isolated; reproduce MOSFET reconnect only if relevant to the acceptance production; integrate only verified essential corrections. Documentation-only update, no flash/merge/bench acceptance implied.


## 4 November 2026 — main and documentation audit follow-up

- **Verified main:** `338e5b630111cc3365bb4c9f923750daaf1596d8` (4 November 19:44:01 UTC). Compared with 3 November `6bf6956cf78c204ce4377c1814f87c523b0d2436`, exactly three commits alter only `firmware/s3-comms-controller/ShowduinoS3CommsController/src/web/WebAssets.generated.h` and `tools/embed-webui/last-build.json`. No active non-generated firmware changes or local-baseline integration landed in that range.
- **Documentation drift:** `main` User Manual and dated finish-line status still lead with the 1 November Emergency-clear failure and omit the subsequent owner report that clearance works in the local copy and the 2 November 22-second show run. The historical failure must remain in the record, but must not be presented as the newest observed state. This draft documentation update is not yet part of `main`.
- **Integration state:** #51 (preserved working local baseline), stacked #52–#58 and #60 (optional FastLED / MOSFET reconnect candidate), and documentation #59 remain open. None of those PRs is installed by virtue of existing on GitHub. The broad FastLED #31 and persistent Emergency #33 remain unvalidated drafts. Do not merge the stack or replace the known-working Emergency path without comparing the flashed local baseline and running physical tests.
- **Latest bench evidence available:** first combined audio/visual show 29 October; local 22-second scenario reported running and local Emergency clearance reported fixed 2 November. There is still no new signed repeatable P4 LOAD → START → COMPLETE evidence, nor documented mid-show Emergency assert/release/authorised clear/no-auto-resume acceptance. Director resets/Cue Library navigation and MOSFET output drop during Director reconnect require targeted retest, especially if the MOSFET is part of the acceptance show.
- **Minimal non-DMX next actions:** preserve firmware and SD assets; run and log the same complete production twice; perform an isolated mid-show Emergency cycle and verify P4 remains authoritative and stopped after clear; verify MOSFET reconnect only where the show depends on it; promote only tested essential fixes and reconcile the User Manual after source/bench verification. FastLED rollout is optional and outside this gate.


## 3 November 2026 — GitHub main and integration status

**Checked 3 November:** `main` is `6bf6956cf78c204ce4377c1814f87c523b0d2436` (14:10:56 UTC). The two commits since the 2 November 15:47 baseline are labelled **Regenerate S3 Studio WebUI bundle**; no merged local-firmware reconciliation is evidenced by these commits. PR #51 (working local baseline) remains a draft based on `main`; PRs #52–#58 and new #60 (Audio opt-in FastLED RGB frames) form a dependent draft integration chain, **not main**. Documentation PR #59 remains open. These statuses do not establish installed hardware behaviour.

**Latest physical evidence available:** 29 October first combined audio/visual playback; owner subsequently reports local Emergency clearance fixed and a local 22-second show run. No new serial logs or signed repeat LOAD → START → COMPLETE / mid-show Emergency acceptance have been provided. Director stability, Cue Library navigation and the reported MOSFET output drop on Director reconnect still require controlled verification. The local working Emergency implementation must be preserved over any untested PR variant.

**Minimum non-DMX path:** snapshot exact flashed firmware and show assets; validate one complete repeatable run and P4-confirmed completion; exercise Emergency assert, physical release, authorised clear and no auto-resume; isolate and test the MOSFET reconnect regression if used in that show. Only promote verified essential fixes from draft branches; defer optional FastLED rollout and new features. This dated update supersedes older PR-status wording below without rewriting historical test evidence.

## 2 November 2026 — working local baseline and staged GitHub integration

**Source snapshot:** GitHub `main` at `6b0fb4e4cf7418ca3350c2a066e91336afb0a9a5` (2 November, 15:47 UTC). Compared with `0e3d0786` (1 November, 22:00 UTC), the 23 intervening commits change documentation and generated Communications S3 WebUI assets/manifest only; **no non-generated active firmware implementation changes**. PR #46's documentation correction is merged. The newer local/PR firmware is **not on main**.

**Latest owner-reported physical state:** The user reports that Emergency clearance is fixed in the local flashed copy, the existing 22-second full-node production runs, and the active nodes are working apart from the deferred C3 LED-only node. This updates the 31 October `AWAITING STAGE` failure report, but it does **not** independently establish repeated clear/no-resume safety acceptance, exact flashed hashes, or a signed full-scenario LOAD → START → COMPLETE record. Preserve the working local folder and assets; don't overwrite it with older main or older PR #47–#50 sources.

**Newly reported behavior to test:** The actual MOSFET load reportedly drops to zero when the Director reconnects even though P4 and Communications remain powered. This is an owner-reported regression, not a confirmed root cause. Draft [#58](https://github.com/sumkindafreak/new_showduino/pull/58) adds candidate MOSFET hold/restore logic and a matching P4 Emergency discovery/keepalive guard. It is not on main; tests/compilation and physical reconnect, node reboot and Emergency gates must pass before any deployment. The MOSFET channel driving the smoke effect must not be assumed reliable on reconnect.

**Integration candidates, all still draft and not merged:**

| Candidate | What exists | Evidence and limitations |
| --- | --- | --- |
| [#51](https://github.com/sumkindafreak/new_showduino/pull/51) | Preserves 904-file local working firmware snapshot, plus bounded SHDO `action.value` and long-command fixes | Host suites and reference P4/Director builds pass; no flash, installed settings/SD identities or physical acceptance |
| [#52](https://github.com/sumkindafreak/new_showduino/pull/52) | Isolated shared FastLED strip adapter, not enabled by any existing node | Host contracts and API probes pass; no bench output |
| [#53](https://github.com/sumkindafreak/new_showduino/pull/53) | Opt-in S3 Lamp FastLED with Adafruit legacy default | Renderer parity and both reference builds pass; no Lamp hardware sign-off |
| [#54](https://github.com/sumkindafreak/new_showduino/pull/54) | LED dependency map and unchanged Audio/Emergency/Comms/MOSFET reference builds | Compile evidence only; Audio's native RMT transport cannot be replaced wholesale |
| [#55](https://github.com/sumkindafreak/new_showduino/pull/55) | Opt-in Director ambient FastLED; Emergency locator priority correction; Audio boundary design | Synthetic parity and reference builds pass; no Director physical sign-off |
| [#56](https://github.com/sumkindafreak/new_showduino/pull/56) | Tests for actual Audio native LED output, no firmware conversion | Host/sanitizer tests pass; physical timing/audio concurrency untested |
| [#57](https://github.com/sumkindafreak/new_showduino/pull/57) | Audio RGB/GRB packing boundary retaining native RMT transport | Reference builds and trace parity pass; no FastLED Audio renderer enabled or physical validation |
| [#58](https://github.com/sumkindafreak/new_showduino/pull/58) | MOSFET reconnect/steady recovery plus P4 Emergency ownership guard | Candidate with local tests; pending CI/bench validation; node and P4 must be reviewed/deployed together |

These are **stacked pull requests**: #51 targets main, and each of #52–#58 targets the preceding integration branch. Their PR descriptions and successful CI runs are evidence of implementation/compilation, **not evidence of installed firmware or safe hardware behavior**. Older broad all-node FastLED [#31](https://github.com/sumkindafreak/new_showduino/pull/31) is an unmerged experimental draft; do not substitute it for the staged review.

**Smallest non-expansion finish-line gates:**

1. Preserve the exact flashed local baseline, P4/Director/Comms/node version and configuration records, and the working SD production/assets. Keep candidate changes off the bench until reviewed.
2. Repeat the *same* 22-second full-node production, recording P4 LOAD → START → COMPLETE, expected audio/pixels and MOSFET smoke command timing; record any playback or Director UI regressions.
3. With hazardous effect loads disconnected, verify the local Emergency fix: independent P4 latch, safe outputs, physical release, deliberate Director-confirmed clear, no automatic show resume; repeat after lost messages/reconnect.
4. Reproduce the MOSFET output-drop case during Director-only reconnect, and test node reboot and Emergency with the paired candidate on a controlled isolated bench **before** accepting #58.
5. Reconcile #51 against current main, then integrate only changes required for reliable completion; defer optional FastLED conversion and other enhancements until after the acceptance record. Update the manual baseline and feature matrix only after the corresponding source and tests are merged.

**Release classification:** first audio/visual show **achieved** (29 October); local 22-second full-node show **owner-reported running** (2 November); repeatable complete-scenario and Emergency recovery **not formally accepted**. The historical 31 October failure remains recorded below, not silently erased.

## 1 November 2026 follow-up — Emergency clear remains unverified; Cue Library navigation defect

- **Repository baseline:** `main` at `0e3d0786b6275783c2b1a0815ab2dcb51d1bbf3d` (1 November 22:00 UTC). Compared with `d5d8f2e7b629` (31 October), four new commits only regenerate the S3 Communications embedded `WebAssets.generated.h` and `tools/embed-webui/last-build.json`; **no active firmware source changed** in this interval. These commits cannot establish that the Director or Emergency regression has been fixed.
- **Additional owner bench details:** The Director's **CLEAR EMERGENCY** control has remained on **AWAITING STAGE** after an E-stop request, with Emergency latched until reboot on a failed attempt. The owner also reports that navigating away from **Cue Library** can leave the previous screen visible rather than clearing/rebuilding the next page. The system temporarily appeared to recover during testing, but this is **not** repeatable Emergency-clear acceptance or a confirmed regression fix. Treat the two symptoms as separately reproducible faults until logs show a common cause; do not assume Cue Library caused Emergency failure.
- **Priority:** Emergency clear and Director responsiveness remain **FAIL/BLOCKED** for the non-DMX finish line. Preserve the exact flashed binaries and serial logs; with effect loads isolated, trace Director confirmation → Comms UART/ESP-NOW forwarding → P4 clear decision/state reply → Director state update. Verify the physical Emergency input is released and any station blocker is cleared only by authorised P4 logic. Never bypass the latch or use reboot as a normal clear. Once fixed, test Cue Library navigation separately, then repeat one complete scenario and its mid-show Emergency acceptance.
- **Release planning note:** The owner's current intended launch window is **February 2027**; older issue #42 text identifying Halloween 2026 is a historical target, not evidence of a completed release. This finish-line audit concerns one complete non-DMX scenario and does not expand into a broader release feature list.
- **Documentation disposition:** This is a dated field-evidence update on PR #46, not a firmware correction or a new claim of physical sign-off. The 29 October first combined audio/visual playback remains achieved.

## 31 October 2026 follow-up — safety regression blocks finish-line acceptance

- **Current GitHub `main`:** `d5d8f2e7b629` (31 October 18:19 UTC). Compared with 30 October `3d4790961e5b`, four commits change only generated S3 Comms `WebAssets.generated.h` and `tools/embed-webui/last-build.json`; no active firmware implementation changes in that range. Embedded bundle: 42 assets, 214,954 bytes, build hash `56677901c6a2`. This does **not** identify the firmware actually flashed onto any board.
- **Owner-reported physical regression, 31 October:** Following recent show/Cue Library testing, Emergency sometimes **does not clear**, and the Director sometimes **becomes unresponsive until reset**. These are observations, not a root-cause finding. Do not attribute the failure to Cue Library or any GitHub commit without serial/backtrace evidence. An Emergency latch that stays active until an authorised clear is correct; inability to clear after release and confirmation is not acceptable. A Director reset is not a safety-clearance method.
- **Acceptance decision:** The 29 October first combined audio/visual show is still a confirmed milestone. Repeatable full LOAD → START → COMPLETE is not formally signed. Emergency clear/recovery and Director availability are **FAIL/BLOCKED**, so the non-DMX scenario and full v1 release remain **NOT READY**. Do not connect hazardous external effect loads or operate for guests while this remains unresolved.
- **Smallest diagnostic sequence:** (1) preserve current working flashed builds, production and assets; (2) capture synchronized or timestamped Director, S3 Comms and P4 serial output during one controlled low-power Emergency assertion, physical button release and Director-confirmed clear attempt; (3) determine whether Director input/confirmation, Comms transport, P4 acceptance/rejection or station identity/latch persistence is preventing clearance; (4) capture exact Director crash/lockup context and decode any panic against the flashed ELF; (5) implement only a verified correction, then repeat Emergency latch → authorised clear → all outputs safe/no auto-resume and repeat the same complete show. Do not weaken or bypass Emergency to obtain a pass.
- **Documentation:** This 31 October addendum is owner bench evidence added to documentation PR #46. No firmware fix, physical retest, or safety acceptance is claimed by the documentation change.

## 30 October repository follow-up (after documentation merges)

- **Current `main` checked:** `3d4790961e` (30 October 2026, 18:21 UTC). Compared with the 20 October manual firmware-source baseline `f4c9aea`, GitHub reports 47 commits. The changed paths are documentation, the generated S3 `WebAssets.generated.h` and its build manifest, plus documentation READMEs; **no non-generated active firmware implementation files changed in that comparison**. This does not identify the versions actually flashed on the bench.
- **Documentation landed on `main`:** PR #41 (24–29 October finish-line audit), PR #44 (first-show reconciliation) and PR #45 (Website Studio Cue Library/Director limitations) are now **merged**. Previous wording calling #41/#44 proposed or unmerged belongs to its dated historical snapshot, not the current repository state.
- **Generated S3 bundle:** build hash `56677901c6a2`, 42 assets and 214,954 embedded bytes remain unchanged; build timestamp advanced to 30 October 18:21 UTC. A regenerated header is not proof of a new tested firmware release or a changed Studio payload.
- **Documentation correction:** the manual index's merged Cue Library entry had accidentally swallowed a duplicate Quick Start link. The 30 October follow-up corrects that rendering error and removes an obsolete duplicate milestone paragraph from the root README.
- **Open versus closed draft fixes (30 October):** PR #35 (SHDO `action.value` parsing) and PR #37 (WAV stutter buffering) are **closed without merge**, not implemented on `main`. PR #36 (long-command rejection) is still open and unmerged, based on the #35 branch; PR #29 (22-second integration fixture), PR #33 (Emergency persistence) and PR #40 (ten-pixel defaults) remain open/unmerged. Do not equate a closed PR with an installed fix or undo the owner's separately observed clean WAV playback.
- **Bench status unchanged since the 29 October report:** combined audio/visual playback achieved, but no new signed complete-scenario or mid-show Emergency record was found in the checked repository/issue #42. Keep Director crash, asset-selection and microphone acknowledgement concerns open until retested on the exact installed build.

## Sources and scope

Repository: `sumkindafreak/new_showduino`; `main` observed at `a85f3cf8050118dbc69e363bfa70a212cfd8e48c` on 30 October. The recent visible commits regenerate the S3 embedded Studio WebUI bundle; they do not by themselves prove a physical firmware change. This is a documentation-only reconciliation, not a new firmware test or a complete code-to-manual certification.

## Verified evidence levels

- **Owner-reported physical milestone (29 October):** Showduino played its first show combining audio and visual effects. Some teething issues remain. Do not describe the system as never having played a show.
- **Previously owner-reported partial bench successes:** WAV audio and volume behaving correctly on the installed Audio Node; working P4 and Audio Node pixel effects. Exact installed firmware commits have not been recorded in this document.
- **Not formally accepted:** existing 22-second all-node production through LOAD → START → COMPLETE; complete timing and node-output record; Emergency asserted during audio, all outputs safe, Director-authorised clear, and no automatic restart.
- **Unresolved or not retested:** Director Core 1 LoadProhibited resets observed 28 October; microphone CALIBRATE and threshold acknowledgement; installed firmware version matching; remaining SHDO deployment and node compatibility findings. Do not infer that any unmerged fix is installed.

## Smallest non-DMX finish-line actions

1. Preserve and identify the exact firmware, SD assets and show used for successful playback.
2. Repeat the same production, recording LOAD, START, expected cues, COMPLETE, audio continuity and node output observations.
3. Reproduce and fix Director resets only if they remain present on this configuration.
4. Repeat with Emergency asserted mid-playback; record independent P4 latch, safe outputs, deliberate authorised clear and no automatic restart.
5. Sign the acceptance record and only then update release readiness claims.

## Documentation rules

The 20 October manual source baseline is retained as historical evidence, not silently relabelled as current. Keep implemented-in-main, proposed-on-PR, flashed-on-bench, observed-on-hardware and formally-accepted distinct. The Director selects and requests shows; P4 owns runtime and Emergency decisions; Comms transports; specialist nodes act and report. Emergency cannot depend on Director connectivity. Do not add DMX to this finish-line scope.

## Remaining documentation work

This reconciliation updates the operator entrypoints and records the first-show milestone. It does not claim to have checked every firmware symbol, pin mapping, document, release note, archived design or example in the repository. A full source-by-source documentation audit is still required before calling all repository documentation current.
