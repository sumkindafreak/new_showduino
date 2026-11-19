# SHOWDUINO V1 — Pre-Opening and Bench Acceptance Checklist

**19 November 2026 · Source reference `41c7c71` · Product `1.0.0-rc.1`**  
**Use:** Before guests enter **and** as the record for the initial non-DMX physical acceptance of the actual flashed installation.

> **Not yet approved for public operation on documentation alone.** Owner-reported playback and Emergency-clear fixes are not a signed full safety pass. Keep hazardous effect loads disconnected until site-specific electrical and operational checks have been completed by competent personnel. Showduino's theatrical Emergency is not a certified fire/life/machinery safety device.

**Date:** __________________  **Venue/Production:** __________________  
**Operator:** __________________  **Witness:** __________________  
**P4 firmware/build:** __________________  **Comms:** __________________  
**Director:** __________________  **Node IDs and versions:** __________________  
**Source/installed mismatch noted?** YES / NO — reference: __________________

Mark each item **PASS / FAIL / N/A**, attach recorded evidence and sign. A blank item is **not** a pass. **N/A** only where the production does not require that item.

## A. Independent site safety and power

- [ ] Site-required fire, evacuation, machinery and mains isolation systems have been verified **independently of Showduino**.
- [ ] Hazardous smoke, motion, mains and high-power output loads are isolated for initial tests; safe test loads are identified.
- [ ] Cabinets, PSUs, fuses, wires, strain reliefs, connectors, antennas and thermal condition are inspected.
- [ ] LED supply current, power injection, common ground and signal conditioning are adequate **including Emergency bright white**.
- [ ] Main P4 momentary Emergency button is physically reachable, undamaged, released and correctly wired.
- [ ] Production media, P4 SD and required node SD cards are in place with backed-up known-good versions.
- [ ] Exact **flashed** firmware versions, firmware source hashes, compiler/core/libs and device settings are recorded.

## B. Boot, links and readiness

- [ ] P4 boots and exposes normal authoritative state.
- [ ] Comms S3 boots; **GPIO17 TX → P4 GPIO4 RX / GPIO18 RX ← P4 GPIO5 TX**, 115200 8N1 link confirmed.
- [ ] Director links to Comms and receives fresh P4 state; does not falsely claim RUN readiness with a stale link.
- [ ] All **required** Audio, Lamp, Pixel, MOSFET and Emergency nodes identify correctly and report healthy state; mark each optional node N/A only if truly unused.
- [ ] P4 can list the known-good SD production; no storage or Emergency fault is present.
- [ ] Director backlight wake, navigation, touch and Cue Library close/return function repeatedly **without freezes or unwanted resets**.
- [ ] Optional Comms SoftAP/browser works; **P4 OFFLINE** indication is truthful. Venue Wi-Fi/Internet are not mistakenly required for a local run.

## C. Baseline show — **two consecutive complete runs**

Use the **same known-good 22-second (or explicitly identified existing) production**, with safe low-power effects.

| Evidence | Run 1 | Run 2 |
| --- | --- | --- |
| Start timestamp | | |
| P4 LOAD accepted, production ID | | |
| P4 START accepted | | |
| Cues/time progression logged | | |
| Audio Node playback observed | | |
| P4/Audio or C3 show pixels observed | | |
| MOSFET safe-load state observed (or N/A) | | |
| P4 COMPLETE recorded | | |
| Output safe at end | | |
| PASS / FAIL | | |

- [ ] Both runs show P4-confirmed **LOAD → START → COMPLETE**, not just a UI animation.
- [ ] Required physical outputs and node identities agree with the cue routing.
- [ ] No unexpected Director reboot, running-show output loss, automatic restarts or missing node readiness occurs.
- [ ] The known-good firmware and production remain backed up; no optional DMX/FastLED/new-lamp feature introduced to satisfy the gate.

## D. **Separate** Emergency interruption and deliberate clear

Run a controlled production and interrupt it with **the main P4 physical button**.

- [ ] Press causes Emergency latch (after configured debounce); Director and P4 agree on the state.
- [ ] Timeline stops progressing; hazardous outputs are safely off/isolated; MOSFET and each fitted node follow their defined Emergency rule.
- [ ] GPIO23 Show Pixels, GPIO24 signage and fitted programmable node pixels follow their approved Emergency-white policy.
- [ ] Programme audio stops/does not autonomously resume; P4 Emergency audio, if available, is **not** a dependency for safety.
- [ ] **Release** main physical button: Emergency remains **latched**.
- [ ] With button held for **8 seconds**, Locate may be tested **separately**; acknowledging Locate **does not** clear Emergency or click through the overlay.
- [ ] **Director Emergency → CLEAR EMERGENCY → CONFIRM CLEAR** produces a P4-authorised result **only after button release**.
- [ ] A pressed button/new assertion rejects or cancels stale clearance; no manual or browser shortcut bypasses P4.
- [ ] After clear, signage returns to its normal indication and show output returns to its defined safe state.
- [ ] **NO automatic timeline or programme-audio restart**. Any further playback needs a fresh operator decision.
- [ ] No Director hang, **AWAITING STAGE** deadlock or reset-to-clear workaround occurs.

**Emergency evidence:** Timestamp __________ / P4 log __________ / Director log __________ / Comms log __________ / photo/video __________ / PASS-FAIL __________

## E. Required node-specific checks

- [ ] **Audio:** correct A161/ES8388 board, local file, audible output, correct reported status, no unintended playback after Emergency.
- [ ] **Pixels:** each installed line wired/sized correctly; default 10 programmable pixels versus fixed indicators understood; normal/blackout/Emergency behaviour tested.
- [ ] **Lamp:** installed S3 lamp's ignition/controlled mode/emergency behaviour tested. **Simplified C3 lamp design is not a pass for an S3 or proposed C3 firmware.**
- [ ] **MOSFET:** Actual feedback and physical safe-load voltage agree; slider alone does not drive output; APPLY, OFF/ALL OFF and Emergency safe-off tested.
- [ ] **MOSFET reconnect regression:** holding a permitted output through a **Director-only reconnect** does not unexpectedly drop/restart; verify P4 remains authoritative.
- [ ] **Wireless Emergency:** each installed station asserts P4; none can remotely clear; loss-of-link policy and local rearm validated.
- [ ] **P4 I/O/I²C plugins:** address, voltage, bus ownership and required output behaviour checked or N/A.

## F. Authoring/deploy verification (separate from original fixture)

- [ ] Website Studio SHDO v2 exported and labelled with compatible source/firmware versions.
- [ ] Comms System Console **Persist SHDO v2 to P4 SD** obtains real P4 acceptance; persisting never auto-starts.
- [ ] Loaded compiled production matches intended cue types/segments/assets. Rejection is recorded, not bypassed.
- [ ] Embedded `/studio/` snapshot/version distinguished from public Website Studio.
- [ ] Unsupported authoring presets/legacy Relay, generic FX, DMX are not claimed as commissioned outputs.

## G. Result and release decision

| Gate | Result |
| --- | --- |
| Site-independent electrical/life safety | PASS / FAIL / INCOMPLETE |
| Two controlled complete runs | PASS / FAIL / INCOMPLETE |
| Full Emergency recovery and no auto-resume | PASS / FAIL / INCOMPLETE |
| All required nodes and load-specific checks | PASS / FAIL / INCOMPLETE |
| Director stability and reconnect behaviour | PASS / FAIL / INCOMPLETE |
| Installed firmware/SD backups and records | PASS / FAIL / INCOMPLETE |

**Overall:** ☐ **OPEN** ☐ **DO NOT OPEN**  
**Reason / outstanding defects:** ________________________________________

**Operator signature:** __________________  **Witness/approver:** __________________

If any safety item fails or is incomplete, mark **DO NOT OPEN**. A source review, PR merge, screenshot, bench anecdote or CI-green result is not a physical acceptance record. Retest after every relevant firmware/hardware change.

**References:** [User Manual](SHOWDUINO_USER_MANUAL.md) · [Physical test plan](../physical-test-checklist.md) · [Parts Register](SHOWDUINO_BUILD_AND_PARTS_REGISTER.md) · [Gaps](SHOWDUINO_MANUAL_GAPS.md) · [Source Matrix](SHOWDUINO_MANUAL_SOURCE_MATRIX.md).
