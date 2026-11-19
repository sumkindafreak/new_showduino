# Showduino V1 — Documentation Rewrite Review Record

**Date:** 19 November 2026 · **Product:** `1.0.0-rc.1`  
**Reviewed firmware repository:** `sumkindafreak/new_showduino` [`main` at `41c7c71ac617abdf9feeedf2064e6b5a9f31cc15`](https://github.com/sumkindafreak/new_showduino/tree/41c7c71ac617abdf9feeedf2064e6b5a9f31cc15).

## Why this rewrite was necessary

The maintained manuals and 20 October source matrix did not reflect the **18 November merged source reconciliation** ([PR #72](https://github.com/sumkindafreak/new_showduino/pull/72)). Historical banners were stacked before operating procedures; the Studio Cue Library guide incorrectly stated the Director tile was disabled; some web help copy still describes an obsolete Emergency clear gesture; and the production-format reference lagged the active P4 parser.

[Documentation PR #73](https://github.com/sumkindafreak/new_showduino/pull/73) was **open** at the start of the rewrite. Its dated [18 November source-reconciliation evidence report](SHOWDUINO_FINISH_LINE_STATUS_2026-11-18.md) is copied unchanged into this branch so that a later merge of this rewrite cannot discard that evidence. This branch is based on GitHub `main`, and must be reviewed for potential conflicts with #73 before merge.

## Direct code/source reviewed

- P4: `BoardConfig.h`, `ShowRuntimeOwner.h`, `src/EmergencyInput.cpp` and `src/ProductionFormat.cpp`; supporting production/SD architecture documents.
- Communications S3: `BoardConfig.h`, WebUI/SoftAP/OTA documentation and the canonical Console source `web/showduino-studio/`.
- Director: `BoardConfig.h`, `DisplayPages.h`, `page_01_home.cpp`, `DirectorCueLibrary.cpp`, `DirectorEmergencyClearFlow.h` and `DirectorEmergencyClearDialog.cpp`.
- Protocol: `showduino_version.h` and `showduino_pixel_defaults.h`.
- Web Console: `js/pages/Productions.js`, `Live.js` and `Network.js`.
- Current architecture, production, emergency, pixel/audio, S3 Lamp, C3 Pixel, C3 Emergency and MOSFET reference documents.

This was a **repository source review**, **not** a full inspection of the author's local firmware/SD, a deployed Website Studio release, physical hardware or every archived/experimental sketch. Claims sourced through specialist documentation are differentiated from direct code inspection in the [Source Matrix](SHOWDUINO_MANUAL_SOURCE_MATRIX.md).

## Maintained documents rebuilt or reconciled

| Document | Main correction |
| --- | --- |
| [User Manual](SHOWDUINO_USER_MANUAL.md) | Unified RC operating guidance, production lifecycle, emergency and nodes; removed obsolete banner stack |
| [Quick Start](SHOWDUINO_QUICK_START.md) | P4 readiness vs S3 connectivity, safe show run, SHDO compile/persist versus load/start |
| [Director Guide](SHOWDUINO_DIRECTOR_OPERATOR_GUIDE.md) | Current page names, enabled native Cue Library, safe outputs and deliberate Emergency clear |
| [Studio Cue Library](SHOWDUINO_STUDIO_CUE_LIBRARY_GUIDE.md) | Website preset editor distinct from Director monitor and embedded snapshot |
| [Pre-Opening Checklist](SHOWDUINO_PRE_OPENING_CHECKLIST.md) | Two P4-confirmed runs, separate Emergency and actual physical-load/signature gates |
| [Parts Register](SHOWDUINO_BUILD_AND_PARTS_REGISTER.md) | Actual installed PCB revision, toolchain, wiring, backups and safe replacement |
| [First Show Guide](SHOWDUINO_FIRST_SHOW_AND_DOCUMENTATION_GUIDE.md) | Preserve local known-good bench, capture repeatability before adding features |
| [Source Matrix](SHOWDUINO_MANUAL_SOURCE_MATRIX.md) | Current source lineage with CODE/SOURCE-DOC/HARDWARE-PENDING distinctions |
| [Gaps](SHOWDUINO_MANUAL_GAPS.md) | Traceable legacy gap IDs and new UI/production discrepancies |
| [Manual Index](README.md) | One current entry point and correct release gate |
| [Root README](../../README.md) | Acknowledge merged #72, remove stale “all candidates unmerged” opening notices |
| [Production Storage](../production-storage.md) | Current P4 format-v1 ESTOP pixel route and SHDO persistence contract |

## Key source findings

1. **Merged source vs flashed local build:** #72 is merged and passed host checks, but a different local binary may be running. No automatic flash replacement is authorised.
2. **Director Cue Library:** `page_01_home.cpp` enables its tile and `DirectorCueLibrary.cpp` implements FOLLOW/BROWSE/START SCENE. The historical disabled-tile instruction is false in reviewed `main`; installed UI remains unverified.
3. **Emergency:** GPIO25 LOW assertion latches; eight-second hold is **LOCATE**, not clear. Clear requires released input, Director confirmation and P4 authority. **Never auto-resume**.
4. **Web UI-copy defect:** `web/showduino-studio/js/pages/Live.js` references a “3-second hold window.” This conflicts with the P4/Director workflow and should be corrected by a separate **WebUI** change with appropriate tests, not misreported in the manual.
5. **Production format:** SHDO v2 is portable authoring; the P4 stores compiled format v1 and does not auto-load/start. `src/ProductionFormat.cpp` includes `ESTOP:NODE:PIXEL:` bounded route omitted by the older production-storage guide.
6. **Node distinctions:** S3 Lamp active vs simplified C3 Lamp design; wireless Emergency is assert-only; Audio Node GPIO22 pixels stay on the same identity; 10 LED programmable defaults do not apply to fixed indicators.

## Physical evidence and unresolved test scope

**Owner reported:** first coordinated audio/visual show 29 October 2026, working 22-second local show and locally fixed Emergency clearance.

**Not signed off:** two back-to-back P4-confirmed LOAD → START → COMPLETE runs; separate Emergency interruption through safe outputs → physical release → authorised clear → no restart; Director navigation/reset regression; MOSFET output through Director-only reconnect; fitted hardware/voltage/pin verification; end-to-end Website Studio deployment on exact installed binaries.

No attached physical equipment was used. No certification, firmware flash, wire change, test pass or customer release has been asserted by this documentation pass.

## Review/merge gates

- Verify all relative Markdown links and change whitespace; ensure headings/anchors resolve.
- Check current P4/Director/Comms source changes and rebase docs if `main` changes before review.
- Review #73 and incorporate any newer valid evidence without deleting it; do not merge conflicting status claims blindly.
- Confirm source-to-prose alignment with [Source Matrix](SHOWDUINO_MANUAL_SOURCE_MATRIX.md).
- Publish docs through a **documentation-only PR**; separately record real physical acceptance in [Pre-Opening Checklist](SHOWDUINO_PRE_OPENING_CHECKLIST.md).
- Mark the [tracking issue #74](https://github.com/sumkindafreak/new_showduino/issues/74) complete only after documentation acceptance and merge. Hardware release acceptance remains a separate gate.
