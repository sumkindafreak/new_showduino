# Showduino V1 — Open Verification and Operating Gaps

**19 November 2026 · Reviewed repository commit `41c7c71`.** This is a **current issue register**. Historical GAP numbers are retained for traceability, not blindly carried forward as current failures.

## Evidence states

- **SOURCE IMPLEMENTED:** active code/contract supports the described operation. No physical pass implied.
- **OWNER REPORTED:** physical observation reported, but the complete logged acceptance record is absent.
- **HARDWARE PENDING:** pin/electrical/timing/end-to-end operation not signed off.
- **DOCUMENTATION/UI MISMATCH:** written or displayed copy conflicts with active source.
- **PLANNED / LEGACY:** not part of the current installed RC operator path.
- **ACCEPTED:** only a dated, reproducible and witnessed relevant test closes a physical gap.

## Known physical evidence

**Owner-reported:** first combined physical audio/visual show on **29 October 2026**; later existing **22-second local show** running and local **Emergency clear** fixed after the 31 October/1 November failures. These achievements are real reports, not “never tested”.

**Still required:** two consecutive logged P4-confirmed LOAD → START → COMPLETE runs; a separate mid-show main-button Emergency → protected outputs → button release → deliberate Director/P4 clear → **no automatic resume** pass. Record exact flashed versions, P4/Comms/Director logs and load feedback. [Acceptance checklist](SHOWDUINO_PRE_OPENING_CHECKLIST.md).

The merged [source integration PR #72](https://github.com/sumkindafreak/new_showduino/pull/72) means that old references treating its constituent fixes as merely unmerged are **outdated**. Source integration and its host tests do **not** sign off flashed hardware. [Docs PR #73](https://github.com/sumkindafreak/new_showduino/pull/73) is separate and was open during this rewrite.

## Numbered legacy gap reconciliation

| ID | Current source/documentation status | Outstanding evidence or action |
| --- | --- | --- |
| GAP-001 main button Locate hold | **SOURCE IMPLEMENTED:** 8-second uninterrupted hold | Physical confirmation of timing and release behaviour |
| GAP-002 Director Locate overlay | **SOURCE IMPLEMENTED:** wake/ambient/first-touch acknowledgement | Touch consumption and Emergency retention on hardware |
| GAP-003 Director/P4 Emergency clear | **SOURCE IMPLEMENTED; OWNER REPORTED local fix** | Full safe physical request → confirm → response/no-auto-resume pass |
| GAP-004 Emergency across power loss | **OPEN POLICY / SAFETY GAP** | Decide, implement and test required reboot/power-loss latch policy; never claim persistence |
| GAP-005 Emergency acceptance | **HARDWARE PENDING** | All relevant nodes, safe outputs, no auto-resume, no reset workaround |
| GAP-006 first-owner Studio journey | **PARTIAL** | Verify current public Website Studio export vs installed P4 compiler end to end |
| GAP-007 persistent theatrical cue set | **PARTIAL / DOC MISMATCH** | Current P4 parser now includes `ESTOP:NODE:PIXEL:` route; old production-storage prose must be updated; test cue support against flashed build |
| GAP-008 multiple “Studio” surfaces | **DOCUMENTED DIFFERENCE** | Maintain Website vs Comms Console vs embedded `/studio/` version parity; avoid overpromising |
| GAP-009 bench Wi-Fi credential | **OPEN PROVISIONING** | Replace public/default bench secret and record private venue provisioning method |
| GAP-010 network commissioning | **SOURCE IMPLEMENTED / HARDWARE PENDING** | Prove SoftAP, STA/channel follow, Director/node reconnect on venue network |
| GAP-011 OTA | **Comms self-OTA only** | Physical slot/update/rollback tests; never claim system-wide OTA |
| GAP-012 version manifest alignment | **REQUIRES RELEASE AUDIT** | Source/manifest/flashed binaries and tags need exact reconciliation |
| GAP-013 Director OS2 | **DISABLED in default build** | Do not describe as current interface |
| GAP-014 active Director pages | **SOURCE IMPLEMENTED / HARDWARE PENDING** | Screen labels/navigation/readiness/Cue Library runtime tests |
| GAP-015 P4 GPIO23 pixel line | **SOURCE IMPLEMENTED / HARDWARE PENDING** | Test power, 10+ LED count, segments, effects and Emergency white |
| GAP-016 Node maturity | **MIXED** | Per-device input/output, recovery, permissions and load verification |
| GAP-017 wireless Emergency stations | **ASSERT-ONLY / HARDWARE PENDING** | Physical wiring/rearm, radio loss and P4-latch assertions, no remote clear |
| GAP-018 enclosure connectors | **INSTALLATION-SPECIFIC** | Photograph/label pinouts and voltage ratings for supplied hardware |
| GAP-019 certification | **UNSUPPORTED CLAIM** | Independent site safety systems and competent acceptance; no certification marketing |
| GAP-020 Director touch calibration | **SOURCE IMPLEMENTED / HARDWARE PENDING** | Verify five-point UI, NVS persistence, abort on Emergency |
| GAP-021 ESTOP Node pixel inventory | **PARTIAL / SURFACE PARITY** | Verify Website Studio and embedded inventory route on installed versions |

## New/reopened source discrepancies (19 November)

| Ref | Finding | Operator consequence | Ownership |
| --- | --- | --- | --- |
| **DOC-022** | `web/showduino-studio/js/pages/Live.js` still says a browser clear cannot bypass a **physical 3s-hold window**, inconsistent with P4's 8-second **Locate**, and Director-confirmed clear | Browser help text **must not** be followed as a clear procedure | Tracked as [issue #76](https://github.com/sumkindafreak/new_showduino/issues/76); UI-copy correction required in a **separate** WebUI change with tests |
| **DOC-023** | Older `SHOWDUINO_STUDIO_CUE_LIBRARY_GUIDE.md` described the Director Cue Library tile as disabled; reviewed `page_01_home.cpp` enables it and `DirectorCueLibrary.cpp` has monitor/scene controls | Current rewritten guide states **enabled in source**, **hardware pending** | Documentation corrected; physical navigation still pending |
| **DOC-024** | `docs/production-storage.md` claimed `ESTOP` unsupported, but reviewed `src/ProductionFormat.cpp` includes `ESTOP` and `ESTOP:NODE:PIXEL:` prefix | Cue compatibility must follow **actual installed code**, not old source note | Documentation reconciliation in rewrite; physical compiler acceptance pending |
| **DOC-025** | The old front matter in README/manuals repeatedly described PRs #51/#52–#60 as unmerged, while their reconciled content is now covered by merged #72 | Remove stale “main unchanged” claims; distinguish merged source and installed firmware | Documentation rewritten; firmware unchanged |
| **BENCH-026** | Historical Director **AWAITING STAGE**, navigation freeze/resets | Prevent release until repeatable clear/navigation test | Hardware retest |
| **BENCH-027** | Historical MOSFET output drop on **Director-only reconnect** | Node outputs must not depend on Director presence | Hardware retest against reconciled source |

## What closes this phase versus hardware release

A **documentation** PR may close **DOC** gaps once every operator claim is traceable to current source, the guides agree, links are checked and outstanding hardware tests are explicit. **No documentation merge** closes an incomplete physical Emergency, electrical or cue acceptance test. Never merge untested firmware merely to make a manual statement true.

**Related:** [Source Matrix](SHOWDUINO_MANUAL_SOURCE_MATRIX.md) · [User Manual](SHOWDUINO_USER_MANUAL.md) · [Pre-Opening Checklist](SHOWDUINO_PRE_OPENING_CHECKLIST.md) · [18 Nov source integration PR](https://github.com/sumkindafreak/new_showduino/pull/72).
