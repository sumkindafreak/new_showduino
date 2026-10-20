# Showduino documentation review — 20 October 2026

## Baselines and limits

- Firmware source: `sumkindafreak/new_showduino`, `f4c9aeaabdcf939417f9891fb01d98e1e5d8aec2`.
- Website source: `sumkindafreak/showduino.com`, `41d3c753491a60212b855019712d342e1eed030c` (Studio membership/indicator update).
- Product: `1.0.0-rc.1`; P4 source `0.6.8`; Communications source `0.5.4`. These are source identifiers, not evidence of the binaries fitted to a particular installation. Release manifests and installed units may differ.
- Manual revision: `0.3-RC`. Review covers the eight maintained manual documents, their operational references, and the public website documentation entry/deployment guides. Historical audits, release snapshots, generated embedded bundles and old per-component setup notes are not promoted to current operator instructions.
- No firmware, authentication, access gates or payment settings changed. No bench acceptance, electrical measurements, certification or physical installation sign-off performed. Existing open safety/hardware gaps remain open.

## Findings corrected

| Finding | Correction | Primary evidence |
| --- | --- | --- |
| September manual baselines and inconsistent component numbers | Record reviewed source/date and current P4/Comms constants; preserve RC status and manifest gap | `protocol/showduino_version.h`; P4 and Comms `BoardConfig.h` |
| MOSFET described as planned in some operator/roadmap prose, implemented elsewhere | Software exists; actual GPIO/load/integrated acceptance remains required | `firmware/mosfet-node-esp32/`; `ProductionFormat.cpp`; `protocol/showduino_shdo.h` |
| Persistent production described as TEST/LOG-only or missing MOSFET | PIXEL/AUDIO/LAMP/MOSFET/TEST/LOG with strict prefix/size bounds; ESTOP-prefixed pixel persistence still unsupported | P4 `src/ProductionFormat.cpp`; production host tests |
| Website deployment guide referred to obsolete `/api/production/import` | Export SHDO, open S3 console, Productions → Persist SHDO v2 to P4 SD → Persist to SD, then separate Load and Start | `web/showduino-studio/js/pages/Productions.js`; `docs/production-storage.md`; website `js/studio-lan-connection.js`, `js/studio-deploy.js` |
| Internet, cloud sign-in or saved address could be mistaken for P4 readiness | Explain verified S3 endpoint versus unverified P4; authoring/preview/save remain separate | website `js/studio-hardware-status.js`, `studio.html` |
| Free and proposed memberships lacked one shared explanation | Free Showduino ID/community; Starter/Pro/Studio proposed; no published plan features/prices or paid activation; hardware entitlements require confirmed terms | website `js/panels.js`, `config/runtime-config.js` |
| Old Lamp audio description in root README | Adafruit Audio FX WAV local effects; programme audio remains on Audio Node | Lamp documentation and active firmware |
| Docs landing page omitted daily checklist and build warning | Link pre-opening checklist and current website workflow; identify legacy Word snapshot | website `docs.html`, `README.md` |
| Operator guide contained escaped code fences | Restore normal rendered code blocks | Director guide |

## Safety and boundaries retained

The main momentary Emergency button asserts after debounce; an uninterrupted eight-second hold additionally requests Locate. Physical release never clears the latch. Director request/confirm is separate; clear does not automatically resume. P4 `EmergencyInput.cpp`, `BoardConfig.h`, `ShowduinoStageEngineP4.ino` and the runtime tests support these statements. Emergency latch restoration after complete power removal remains unproven. No current software test closes physical acceptance gaps.

P4 remains runtime authority. Stored/authorable SHDO, browser preview, RAM commissioning, persistent SD compilation, loading and running are distinct stages. An editable cue is not proof of supported or physically commissioned hardware.

## Validation

- Firmware production host suite passed: parser, storage, runtime/emergency behaviour, upload policy, version comparison, SHDO compiler, WEB/BODY and UART tunnel framing. Includes persistent MOSFET acceptance and rejection of invalid prefixes.
- Website static check passed: 19 pages, 184 asset references, 49 scripts parsed.
- Markdown links and diff whitespace checked before publication; results recorded with the PRs.
- Physical tests and a complete release inventory remain required. Historical component readmes and binary/manual snapshots should be checked against installed firmware before use.

## Maintenance rule

Change Quick Start, User Manual, Director Guide, Pre-Opening Checklist, source matrix and open gaps together whenever an operator workflow changes. Record exact reviewed firmware and website commits. Record physical acceptance separately per installed board. Do not update a release-manifest version or mark a gap closed solely because documentation changed.
