# Showduino V1 — Source-to-Operator Claim Matrix

**Rebuilt 19 November 2026; UI-copy disposition updated 20 November 2026** · Source: [`main` at `41c7c71ac617abdf9feeedf2064e6b5a9f31cc15`](https://github.com/sumkindafreak/new_showduino/tree/41c7c71ac617abdf9feeedf2064e6b5a9f31cc15) · Product `1.0.0-rc.1`.

**Scope:** Source review of current P4, Comms S3, Director, protocol, Comms WebUI and node-specific reference documentation; no local flashed firmware, live website build or physical hardware was accessed. This matrix supports the rewritten operator instructions, **not** a claim of complete bench or certification acceptance.

**Status key:** **CODE** = active source implementation; **ARCH** = architectural contract; **SOURCE/DOC** = backed by specialist documentation, per-node code/hardware verification required; **PARTIAL** = bounded implementation; **UI MISMATCH** = displayed text disagrees with active code; **HARDWARE PENDING** = no signed installed-unit test; **OWNER REPORTED** = dated observation, not full acceptance; **PLANNED/LEGACY** = not a current production instruction.

## Architecture and core configuration

| Operator claim | Exact reviewed source | Status / limitation |
| --- | --- | --- |
| Product `1.0.0-rc.1`, portable SHDO v2 | `protocol/showduino_version.h` | CODE; flashed build unknown |
| P4 authoritative for timeline, show, Emergency | `docs/constitution.md` I–II; P4 `ShowRuntimeOwner.h` | ARCH/CODE |
| Comms S3 transports, hosts SoftAP and WebUI | `docs/constitution.md` III; `firmware/s3-comms-controller/ShowduinoS3CommsController/` | ARCH/CODE |
| Director requests/displays, nodes act | `docs/constitution.md` IV–VII; active firmware roots | ARCH/CODE |
| Loaded show independent of browser/Director/Internet | `docs/constitution.md` IX; P4 `ShowRuntimeOwner.h` | ARCH/CODE; loss tests pending |
| P4 onboard C6 is unused/reserved | P4 `BoardConfig.h`; `docs/architecture.md` | CODE |
| S3 TX17→P4 RX4, P4 TX5→S3 RX18, 115200 8N1 | P4 and Comms `BoardConfig.h` | CODE; physical wiring pending |
| P4 SDMMC and GPIO45 power | P4 `BoardConfig.h`; `docs/p4-sd-storage.md` | CODE; card test pending |
| Main P4 Emergency GPIO25 active LOW pull-up | P4 `BoardConfig.h`; `src/EmergencyInput.cpp` | CODE; physical test pending |
| P4 GPIO23 show pixels, GPIO24 signage | P4 `BoardConfig.h`; `docs/audio-pixel-engine.md` | CODE; LED/power test pending |
| P4 I²C SDA7/SCL8, 3.3 V | P4 `BoardConfig.h`; `docs/plugin-bus.md` | CODE; do not connect 5 V |
| P4 PCM5102A DAC BCLK21/WS20/DOUT22 | P4 `BoardConfig.h`; `docs/audio-pixel-engine.md` | CODE; hardware pending |
| P4 ES8311 is system audio, A1S handles show audio | `docs/audio-pixel-engine.md`; P4 `src/StageAudio.*` | SOURCE/DOC; media test pending |
| Director JC8048W550C 800×480, GT911 SDA19/SCL20 | Director `BoardConfig.h`, `touch_lvgl.cpp` | CODE; PCB rev check |
| Director OS2 shell disabled by default | Director `BoardConfig.h` `SHOWDUINO_OS2_SHELL=0` | CODE |
| Display backlight off is not CPU deep sleep | Director `backlight.cpp` | CODE; wake test pending |
| Programmable line default minimum 10 LEDs | `protocol/showduino_pixel_defaults.h` | CODE; valid saved larger count retained |
| Fixed indicators do not use 10 LED count | `docs/pixel-default-policy.md`; BoardConfigs | SOURCE/DOC; 1 Audio status, 2 Director, 4 MOSFET, 7 Lamp |

## Emergency and Locate

| Operator claim | Exact reviewed source | Status / limitation |
| --- | --- | --- |
| Main button latches after debounce | P4 `src/EmergencyInput.cpp`; P4 `BoardConfig.h` | CODE; HARDWARE PENDING |
| Continuous 8-second hold requests Locate | P4 `src/EmergencyInput.cpp`; `protocol/showduino_emergency_button.h` | CODE; HARDWARE PENDING |
| Physical button never clears Emergency | P4 `src/EmergencyInput.cpp`; `ShowduinoStageEngineP4.ino` | CODE; HARDWARE PENDING |
| Director requests P4 clear then explicit confirmation | Director `DirectorEmergencyScreen.cpp`; `DirectorEmergencyClearDialog.cpp`; `DirectorEmergencyClearFlow.h` | CODE; HARDWARE PENDING |
| P4 rejects clear when button still asserted | P4 `src/EmergencyInput.cpp`; `ShowduinoStageEngineP4.ino` | CODE; HARDWARE PENDING |
| Fresh assertion cancels pending clear | P4 `src/EmergencyInput.cpp` | CODE; HARDWARE PENDING |
| Clear never automatically resumes a show | P4 `ShowRuntimeOwner.h`; `docs/production-storage.md` | CODE; HARDWARE PENDING |
| Node Emergency reconciliation Audio/Lamp | P4 `src/EmergencyInput.cpp` | CODE; other node physical tests pending |
| P4 pixels and signage show bright Emergency white | `docs/audio-pixel-engine.md`; P4 pixel sources | SOURCE/DOC; loaded LEDs pending |
| Locate wake, flashes, first-touch consumption | Director `DirectorLocateScreen.cpp`, `DirectorAmbientPixels.cpp`, `touch_lvgl.cpp` | CODE; hardware pending |
| Latch persists across complete power removal | P4 startup `ShowduinoStageEngineP4.ino` | **NOT PROVEN**; never claim |
| Wireless Emergency stations assert, cannot clear | `docs/emergency-node.md`; `firmware/c3-emergency-node/` | SOURCE/DOC; radio/hardware pending |
| Comms Browser Live PANIC asserts Emergency | `web/showduino-studio/js/pages/Live.js` | CODE; not a normal clear path |
| Previous Live-page “physical 3s-hold” wording | `web/showduino-studio/js/pages/Live.js` and [fix PR #78](https://github.com/sumkindafreak/new_showduino/pull/78) | **SOURCE CORRECTED** in #78 with regression coverage; on-device S3 bundle/physical behaviour still require deployment-specific verification |

## Director, outputs and network

| Operator claim | Exact reviewed source | Status / limitation |
| --- | --- | --- |
| Active Home/Productions/Live/Nodes/Audio/Lamp/Settings/Logs pages | Director `DisplayPages.h`; `page_*.cpp` | CODE; physical UI pending |
| HOME CUE LIBRARY tile is enabled in source | Director `page_01_home.cpp` | CODE; previous “disabled” manual obsolete |
| Cue Library is Follow/Browse/START SCENE monitor | Director `DirectorCueLibrary.cpp` | CODE; not Website editor |
| Cue Library refuses/start guards, stale runtime and Emergency | Director `DirectorCueLibrary.cpp` | CODE; navigation hardware retest pending |
| RUN on Home is link/readiness gated | Director `page_01_home.cpp` | CODE; physical acceptance pending |
| MOSFET Set slider vs Actual, APPLY and safe OFF | Director `page_outputs.cpp`; `docs/director-outputs-mosfet.md` | CODE; no physical load inference |
| MOSFET level changes gated while running/emergency/offline | `page_outputs.cpp`; `docs/director-outputs-mosfet.md` | CODE; physical reconnect test pending |
| Five-point touch calibration saved | Director `DirectorTouchCalibrationScreen.cpp`, `TouchCalibrationStore.cpp` | CODE; hardware pending |
| SoftAP SSID default Showduino, source bench password | Comms `BoardConfig.h` | CODE; credential must change for venue |
| Network Scan/Connect/Disconnect/Forget/AP only | `web/showduino-studio/js/pages/Network.js` | CODE; site channel test pending |
| Internet loss not P4 link loss | `docs/network-gateway.md`; `docs/constitution.md` | ARCH/CODE |
| Browser P4 Offline disables actions, reports P4 state | `web/showduino-studio/js/pages/Live.js` | CODE |

## Studio, production and node actions

| Operator claim | Exact reviewed source | Status / limitation |
| --- | --- | --- |
| Comms System Console distinct from full Website Studio | `web/showduino-studio/README.md` | CODE/documented boundary |
| Embedded `/studio/` is pinned website snapshot | `web/showduino-studio/README.md`; `tools/embed-webui/` | CODE; deployed parity not assumed |
| Website Studio Cue Library has editable presets | `sumkindafreak/showduino.com` (separate repo) | **WEBSITE CURRENT DEPLOYMENT NOT VERIFIED** in this pass |
| SHDO v2 is authoring/exchange contract | `protocol/showduino_version.h`; `docs/studio/production-format.md` | CODE/CONTRACT |
| Console Persist sends SHDO to P4 compile/store | `web/showduino-studio/js/pages/Productions.js`; P4 `src/ProductionFormat.cpp` | CODE; flashed compiler/storage pending |
| Persist does not load/start | `Productions.js`; `docs/production-storage.md` | CODE |
| Separate Load/Start/Pause/Resume/Stop | P4 `ShowRuntimeOwner.h`; Console `Productions.js`, `Live.js` | CODE; state gates apply |
| P4 persistent format v1 manifest/timeline | P4 `src/ProductionFormat.cpp`; `docs/production-storage.md` | CODE; SD physical test pending |
| P4 parser accepts AMBIENCE/PIXEL/AUDIO/LAMP/MOSFET/ESTOP, TEST/LOG | `src/ProductionFormat.cpp` cue types | CODE; earlier docs omitted AMBIENCE and ESTOP |
| ESTOP cue prefix is `ESTOP:NODE:PIXEL:` | `src/ProductionFormat.cpp` | CODE; editor/physical route pending |
| `p4-ambience` stores WAV on P4 SD, emits bounded AMBIENCE cues (including STOP) | `protocol/showduino_shdo.h` `shdoCompileAudio`; `protocol/showduino_ambience_command.h`; P4 `src/ProductionFormat.cpp` | CODE; P4 media and physical playback test pending |
| RAM `/api/studio-timeline` separate from SD persistence | `web/showduino-studio/README.md` | CODE; scoped commissioning |
| Audio ES8388/A161 and local SD | `docs/audio-node.md`; `firmware/audio-node-esp32-a1s/` | SOURCE/DOC; flash and wiring pending |
| Audio GPIO22 pixels under same Audio identity | `docs/audio-pixel-engine.md`; Audio firmware | SOURCE/DOC; physical pending |
| Active S3 carbide Lamp, separate simplified C3 lamp design | `docs/s3-lamp-node.md`; `docs/c3-lamp-node-simplified-integration.md` | SOURCE/DOC vs PLANNED |
| C3 remote Pixel Node | `docs/c3-pixel-node.md`; `firmware/c3-pixel-node/` | SOURCE/DOC; physical pending |
| ESTOP C3 pixel on same ESTOP logical identity | `docs/emergency-node.md`; `firmware/c3-emergency-node/` | SOURCE/DOC; physical pending |
| MOSFET four low-voltage DC channels and feedback | `docs/director-outputs-mosfet.md`; `firmware/mosfet-node-esp32/` | SOURCE/DOC; load test pending |
| Comms self-OTA only, not system-wide OTA | `docs/comms-ota.md`; `docs/system-updates.md` | SOURCE/DOC; rollback physical pending |
| Relay historical, DMX parked | `docs/architecture.md`; `docs/node-roadmap.md` | LEGACY/PLANNED |

## Physical evidence ledger (not source capability)

| Evidence | Status at 19 Nov 2026 | Remaining acceptance |
| --- | --- | --- |
| First combined physical audio/visual show on 29 October | **OWNER REPORTED** | Save dated exact flashed version/log |
| Local 22-second show runs | **OWNER REPORTED** | **Two** consecutive P4-confirmed LOAD → START → COMPLETE passes |
| Local Emergency clear fixed | **OWNER REPORTED** | Safe output state + released button + Director-confirmed P4 clear + **no auto-resume** |
| Director Cue Library/resets | **HARDWARE PENDING** | No freeze/nav regression on installed Director |
| MOSFET reconnect drop | **HARDWARE PENDING** | Safe-load output and P4 state through Director-only reconnect |
| Integration PR #72 | **MERGED SOURCE** | Not physical acceptance |
| Certification and public operation | **NOT ESTABLISHED** | Independent electrical/venue/safety approval |

Source review alone does not establish reliable customer operation. Follow the [User Manual](SHOWDUINO_USER_MANUAL.md), [Pre-Opening Checklist](SHOWDUINO_PRE_OPENING_CHECKLIST.md), [Gaps](SHOWDUINO_MANUAL_GAPS.md) and [Documentation Review](SHOWDUINO_DOCUMENTATION_REVIEW.md).
