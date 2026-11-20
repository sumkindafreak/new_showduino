# Studio Cue Library — Creator Guide

**19 November 2026 · V1 documentation rewrite · Source-reviewed, not physically accepted.**

## Two different Cue Libraries

| Surface | What it is | What it is **not** |
| --- | --- | --- |
| **Website Studio → Timeline → Cue Library** | Search/select/drag preset clips into editable audio, pixel, MOSFET and other lanes | Proof that every palette choice can run on installed P4 |
| **Director HOME → CUE LIBRARY** | **Enabled in reviewed main** as a live cue/scene monitor with FOLLOW, BROWSE and a guarded START SCENE request | A clip authoring/editor or SHDO export tool |
| **Embedded Comms `/studio/`** | A bundled website snapshot built into Comms firmware | Guaranteed same release/features as live Website Studio |
| **Comms System Console → Productions** | Upload/persist compatible SHDO to P4 SD, list/load productions | The complete authoring timeline |

The 30 October statement that the Director tile is disabled is superseded by direct inspection of `page_01_home.cpp` and `DirectorCueLibrary.cpp` at [`41c7c71`](https://github.com/sumkindafreak/new_showduino/tree/41c7c71ac617abdf9feeedf2064e6b5a9f31cc15). The website editor belongs to a separate repository and may change independently.

## Create a cue in Website Studio

1. Open the current Website Studio Timeline and expand **Cue Library** in the track-list header.
2. Search by cue type, action or effect. Choose an appropriate unlocked Audio, Pixel or MOSFET lane, or a compatible Mixed Lane.
3. Drag a preset to the desired position; the editor may snap to its grid. Alternatively double-click to insert at the playhead into an eligible lane.
4. Select the clip. In its Inspector set **start**, **duration**, **logical node**, **physical route/output**, parameters and assets. If no suitable lane exists, add one.
5. For Audio, check the **selected route** before confirming the media location. An `audio-node` cue uses the **Audio Node's SD card**; a `p4-ambience` cue uses the **P4 SD card** under `/showduino/audio/ambience/` and emits `AMBIENCE` commands. Example file names in presets are **not** copied automatically. The actual installed P4 and node firmware must support the chosen route.
6. For Pixel, verify logical segment, line, start LED, count and physical fitted count. P4 GPIO24 is **safety signage**, not a general show line. Programmable lines default to **10** LEDs until a valid larger count is configured.
7. Preview, run **Project Check**, save and export **SHDO v2**. This checks authoring; it does not test the installed hardware.

The source-reviewed catalogue has Audio, Pixel and MOSFET options plus authoring-only/legacy types (including generic FX, trigger and relay). **Do not promise all presets are deployable.** Generic FX, triggers and legacy relay paths must be checked against the actual P4 compiler and live outputs.

## Persist and run the production

1. Back up the known-good installation and check the **installed P4 firmware version** before trying an unfamiliar SHDO.
2. Open Comms System Console → **Productions** → **Persist SHDO v2 to P4 SD**, choose the file and press **Persist to SD**.
3. Read the returned P4 acceptance/rejection. The P4 compiles supported SHDO cue types into its persistent **format-v1** manifest/timeline; a successful upload is **not** a show start.
4. Separately **Load** the production and wait for P4 acknowledgement.
5. Use Director/Live to **Start** a safe test. Observe actual node results and completion. Don't treat a Studio preview as evidence.
6. If the upload is rejected, record the exact error and export. Do not alter a known-good flashed system without isolated regression tests.

Direct current-source evidence: `web/showduino-studio/js/pages/Productions.js`, `src/ProductionFormat.cpp`, `protocol/showduino_version.h`, and [production storage](../production-storage.md).

## Director Cue Library monitor

The Director provides **FOLLOW**, **PREVIOUS**, **NEXT**, **ALL CUES**, **START SCENE**, and **CLOSE**. It displays P4 timeline state, marks stale updates and requires deliberate confirmation before a scene restart request. It is not a drag-and-drop editor. During Emergency it closes; safety authority remains on the P4.

Previous bench reports mention the Cue Library screen remaining visible after navigation. Source support is **not** hardware sign-off; log and retest on the installed build.

## Known source/UI discrepancy

The Comms browser `Live.js` still contains stale explanatory wording referencing a **3-second physical hold**. The current P4 main-button **8-second hold is for Locate**, never clear. Emergency clear requires Director request/confirmation and P4 authority. Treat the browser sentence as a **UI-copy defect**, not an operator instruction; see [Verification Gaps](SHOWDUINO_MANUAL_GAPS.md).

## Related

[Quick Start](SHOWDUINO_QUICK_START.md) · [User Manual](SHOWDUINO_USER_MANUAL.md) · [Director Guide](SHOWDUINO_DIRECTOR_OPERATOR_GUIDE.md) · [Studio production format](../studio/production-format.md) · [Source Matrix](SHOWDUINO_MANUAL_SOURCE_MATRIX.md).
