# Showduino Commercial Manual — Source Matrix

**Manual baseline date:** 16 September 2026  
**Repository:** `sumkindafreak/new_showduino`  
**Baseline branch:** `main`  
**Starting SHA:** `9f304cdc65786c0e8d306987010a97eb5a3ad587`  
**Product:** Showduino `1.0.0-rc.1`

This matrix traces customer-facing manual claims back to the repository snapshot used for this documentation pass.

## Verification language

- **YES — CODE**: active code at baseline directly supports the claim.
- **YES — ARCH**: current architecture/constitution supports the claim and active code is consistent.
- **PARTIAL**: some of the claim is implemented but an important boundary/gap remains.
- **NO — PLANNED**: not a current product feature.
- **LEGACY**: retained source/history only; not normal current product behaviour.
- **HARDWARE REQUIRED**: implementation exists but repository acceptance records still require physical proof.

“Hardware validated?” is intentionally conservative. Source review is not a substitute for bench/installation acceptance.

| Manual section | Feature / claim | Primary repository source | Secondary source | Firmware / code location | Verified current? | Hardware validated? | Notes |
|---|---|---|---|---|---|---|---|
| Cover / baseline | Product is Showduino `1.0.0-rc.1` | `protocol/showduino_version.h` | `releases/showduino-1.0.0-rc.1.manifest.json` | shared protocol header | YES — CODE | n/a | Exact repo SHA is the strongest baseline because component release manifest lags HEAD for P4/Comms. |
| About Showduino | P4 Show Engine is authoritative | `docs/constitution.md` | `README.md`, `docs/architecture.md` | `firmware/stage-engine-p4/ShowduinoStageEngineP4/` | YES — ARCH | HARDWARE REQUIRED | Core constitutional rule. |
| About Showduino | Comms S3 transports, hosts browser UI and proxies APIs; does not decide show state | `docs/constitution.md` | `web/showduino-studio/README.md` | `firmware/s3-comms-controller/ShowduinoS3CommsController/` | YES — CODE/ARCH | HARDWARE REQUIRED | Current dedicated S3 replaces legacy C3 Comms. |
| About Showduino | Director is operator interface, not show authority | `docs/constitution.md` | `docs/architecture.md` | `firmware/director-esp32-8048s050/ShowduinoDirector8048S050/` | YES — CODE/ARCH | HARDWARE REQUIRED | Requests require P4 confirmation/state. |
| About Showduino | Nodes perform specialist physical/media actions | `docs/constitution.md` | `docs/repository-status.md` | active node firmware | YES — ARCH | MIXED | Node maturity varies. |
| About Showduino | Running show does not require browser, Director, venue Wi-Fi or Internet | `docs/constitution.md` | `web/showduino-studio/README.md`, `docs/network-gateway.md` | P4 runtime + Comms proxy | YES — ARCH | HARDWARE REQUIRED | Browser/UI loss is not show-engine loss. |
| Safety | Main unit Emergency input is momentary GPIO25 to GND, active LOW, software-latched | `docs/hardware-pinout.md` | P4 `BoardConfig.h` | `src/EmergencyInput.cpp/.h` | YES — CODE | HARDWARE REQUIRED | 30 ms debounce. |
| Safety | First valid main-button press latches Emergency; release alone does not clear | `src/EmergencyInput.cpp` | `docs/hardware-pinout.md` | P4 emergency input + `triggerEmergency()` | YES — CODE | HARDWARE REQUIRED | Approved product principle matches this part of code. |
| Safety | 8-second continuous-hold Locate on the same Emergency press | P4 `BoardConfig.h` `SHOWDUINO_ESTOP_LOCATE_HOLD_MS` | `src/EmergencyInput.cpp` | `protocol/showduino_emergency_button.h` | YES — CODE | HARDWARE REQUIRED | Gap 001 implemented in firmware; hardware acceptance required. Retired 8-press/6s gesture. |
| Safety | Physical Emergency button never clears | P4 `EmergencyInput.cpp` | Director Emergency screen | P4 clear handshake | YES — CODE | HARDWARE REQUIRED | Gap 003. Retired 3-second hold-to-clear. |
| Safety | Director clear is request → P4 validate → confirm; reject if GPIO25 asserted; new assertion cancels stale confirm | P4 `.ino` | `DirectorEmergencyClearDialog.cpp` | `EMERGENCY:CLEAR` / `CLEAR_REQUEST` / `CLEAR_CONFIRM` | YES — CODE | HARDWARE REQUIRED | USB `EMERGENCY:CLEAR` remains maintenance-only. |
| Safety | Director clear dialog requires deliberate confirmation and says clear does not resume | `DirectorEmergencyClearDialog.cpp` | `DirectorEmergencyScreen.cpp` | active Director UI | YES — CODE | HARDWARE REQUIRED | Exact UI labels verified. |
| Safety | P4 rejects clear while physical button/input remains asserted | P4 `ShowduinoStageEngineP4.ino` | protocol strings | emergency clear handler | YES — CODE | HARDWARE REQUIRED | Pending request + release + timeout checks. |
| Safety | Clearing Emergency does not automatically resume show | `ShowRuntimeOwner.h` | `docs/production-storage.md` | `onEmergencyCleared()` | YES — CODE | HARDWARE REQUIRED | Mid-show runtime remains PAUSED until separate resume/stop action. |
| Safety | Emergency activation pauses active timeline | `ShowRuntimeOwner.h` | `docs/production-storage.md` | `onEmergencyStop()` | YES — CODE | HARDWARE REQUIRED | Preserves timing/cue context but blocks resume while Emergency active. |
| Safety | Emergency latch persistence through complete power removal | P4 `ShowduinoStageEngineP4.ino` | `SHOWDUINO_MANUAL_GAPS.md` | `bool emergencyLocked = false` at boot | PARTIAL / UNKNOWN PRODUCT POLICY | NO | No persistent restore found. Do not claim persistence. Gap 004. |
| Safety / pixels | GPIO24 designated signage normal = first pixel of each 10 green, rest off | `docs/audio-pixel-engine.md` | `docs/hardware-pinout.md` | `src/EmergencyPixels.*` | YES — CODE | HARDWARE REQUIRED | Up to current configured 100 pixels / ten groups. |
| Safety / pixels | Emergency forces GPIO23 and GPIO24 pixels bright white | `docs/audio-pixel-engine.md` | `docs/command-protocol.md` | P4 pixel engines | YES — CODE | HARDWARE REQUIRED | Global pixel emergency policy. |
| Safety / pixels | On authorised clear, signage returns green pattern; show pixels black out; effects do not auto-resume | P4 `ShowduinoStageEngineP4.ino` | `docs/audio-pixel-engine.md` | `emergencyPixelsSetNormal()`, `showPixelsOnEmergency(false)` | YES — CODE | HARDWARE REQUIRED | Clear-to-safe display/output behaviour. |
| Safety / audio | P4 onboard audio is Showduino system/safety audio; attraction audio belongs to Audio Node | `docs/audio-pixel-engine.md` | `docs/audio-node.md` | `src/StageAudio.*`, Audio Node firmware | YES — CODE/ARCH | HARDWARE REQUIRED | No attraction-audio fallback to P4 speaker. |
| Safety / compliance | Showduino is not claimed as certified fire alarm, safety PLC, machinery E-stop, SIL/PL system | `docs/emergency-node.md` | repository-wide absence of certification artefacts | n/a | YES — DOCUMENT BOUNDARY | n/a | Manual avoids unsupported regulatory claims. |
| System overview | Current topology Director → ESP-NOW → Comms S3 → UART → P4 | `docs/architecture.md` | `README.md`, `docs/final-hardware-architecture.md` | Director transport, Comms UART, P4 UART | YES — CODE/ARCH | HARDWARE REQUIRED | Current path, not legacy C3. |
| Hardware tour | Comms UART pins S3 TX17/RX18 ↔ P4 RX4/TX5, 115200 8N1 | Comms `BoardConfig.h` | `docs/hardware-pinout.md` | active BoardConfig files | YES — CODE | HARDWARE REQUIRED | Technical/install information, not normal operator control. |
| Hardware tour | P4 plugin I²C bus SDA7/SCL8, 3.3 V, 100 kHz | `docs/plugin-bus.md` | P4 BoardConfig/plugin source | `src/plugin/` | YES — CODE | HARDWARE REQUIRED | Shared with onboard ES8311. Not a 5 V bus. |
| Hardware tour | P4 GPIO23 = local Show Pixel line, GPIO24 = safety signage pixels, GPIO25 = physical Emergency | `docs/hardware-pinout.md` | `docs/final-hardware-architecture.md` | P4 BoardConfig | YES — CODE | HARDWARE REQUIRED | Final enclosure connector labels not yet frozen. Gap 018. |
| First power-on | Bench/release sequence is P4 → Comms → Director | `docs/physical-test-checklist.md` | `docs/v1-feature-matrix.md` | component boot code | YES — DOCUMENTED | HARDWARE REQUIRED | Commercial PSU/enclosure sequence still needs final hardware acceptance. |
| First power-on | Home/venue Wi-Fi is optional | `docs/network-gateway.md` | `docs/v1-feature-matrix.md` | Comms gateway | YES — CODE | HARDWARE REQUIRED | Local SoftAP remains product access path. |
| Director | Active page set includes HOME, PRODUCTIONS, SHOW DETAILS, LIVE, DIAGNOSTICS, NODES, AUDIO NODE, SETTINGS, AUDIO, SYSTEM LOGS plus system-modal pages | Director `DisplayPages.h` | `ShowduinoUi.h` | active legacy/current shell | YES — CODE | UI HARDWARE REQUIRED | Exact page titles verified from code. |
| Director | OS 2.0 shell is disabled at baseline | Director `BoardConfig.h` | OS2 source tree | `#define SHOWDUINO_OS2_SHELL 0` | YES — CODE | n/a | Do not document disabled OS2 as current. Gap 013. |
| Director / display | Display auto-off is backlight dim/off only; Director remains executing | `backlight.cpp` | Director main loop | backlight service | YES — CODE | HARDWARE REQUIRED | Not MCU deep sleep. |
| Director / display | Auto-off can be disabled with timeout 0; brightness and timeout are configurable | `backlight.cpp`, Director `.ino` | `ShowduinoUi.h` | active Director config | YES — CODE | HARDWARE REQUIRED | UI labels/settings should follow current build. |
| Director / touch | Settings Display CALIBRATE runs a 5-point wizard; valid result is stored in Director NVS and used at boot; factory fallback if absent/invalid | `TouchCalibrationMath.h` | `TouchCalibrationStore.cpp`, `DirectorTouchCalibrationScreen.cpp` | `touch_lvgl.cpp` | YES — CODE | HARDWARE REQUIRED | Gap 020. Not physically accepted. Emergency/Locate abort the wizard without saving. |
| Locate | `DIRECTOR:LOCATE` wakes display, holds backlight, flashes ambient LEDs until first consumed touch | Director `.ino` | `DirectorLocateScreen.cpp` | `DirectorAmbientPixels.cpp`, `backlight.cpp`, `touch_lvgl.cpp` | YES — CODE | HARDWARE REQUIRED | Gap 002 implemented in firmware. 15 s timeout removed. Hardware acceptance required. |
| Network | Comms SoftAP SSID `Showduino`, current bench WPA2 password `showduino` | Comms `BoardConfig.h` | `docs/network-gateway.md` | gateway/web server | YES — CODE | HARDWARE REQUIRED | Code says change bench secret before public venue; production provisioning gap 009. |
| Network | SoftAP remains up in AP-only/AP+STA; optional venue Wi-Fi uses Network page | `docs/network-gateway.md` | `Network.js` | `src/network/CommsGateway.*` | YES — CODE | HARDWARE REQUIRED | Current Network buttons: Connect, Disconnect, Forget, Scan, AP only. |
| Network | Internet loss is not Showduino connection loss | `docs/network-gateway.md` | `Network.js`, constitution | Comms gateway/status | YES — CODE/ARCH | HARDWARE REQUIRED | Key operator distinction. |
| Network | Network page does not display/log saved password | `docs/network-gateway.md` | `networkGatewayDraft.js`, `Network.js` | Comms gateway | YES — CODE | HARDWARE REQUIRED | Password transient in UI draft and NVS storage path. |
| Browser / Studio | Browser UI is hosted by Comms S3 PROGMEM; P4 remains authority | `web/showduino-studio/README.md` | `docs/architecture.md` | S3 web bundle + P4 Web API | YES — CODE | HARDWARE REQUIRED | Browser can remain reachable even if P4 is offline; P4 controls disable. |
| Browser / Studio | S3 system console routes: Home, Productions, Live, Outputs, Devices, Network, System, Settings | `web/showduino-studio/README.md` | JS page files | `web/showduino-studio/js/pages/` | YES — CODE | UI HARDWARE REQUIRED | Settings partial; several pages foundations/expanding. |
| Browser / Studio | `/studio/` full authoring path is not a complete native on-device SHDO workflow | `docs/studio/README.md` | `web/showduino-studio/README.md` | embedded Studio snapshot | PARTIAL | HARDWARE REQUIRED | Gap 006/008. |
| Authoring | SHDO v2 is active portable authoring/interchange contract | `protocol/showduino_version.h` | `docs/studio/production-format.md` | protocol SHDO headers/compiler | YES — CONTRACT | HARDWARE REQUIRED | P4 runtime store is a compiled/subset representation, not direct arbitrary authoring JSON execution. |
| Production deployment | Deploy commit does not auto-load or auto-start | `docs/production-storage.md` | deployment protocol | P4 ProductionDeploy/Store | YES — CODE | HARDWARE REQUIRED | Emergency aborts commit. |
| Production storage | P4 SD path `/showduino/productions/<id>/manifest.json + timeline.json` | `docs/production-storage.md` | `docs/p4-sd-storage.md` | ProductionStore | YES — CODE | HARDWARE REQUIRED | Current runtime format v1. |
| Production storage | Persistent v1 currently accepts TEST/LOG cues only | `docs/production-storage.md` | `web/showduino-studio/README.md` | production parser | YES — CODE | HARDWARE REQUIRED | Major commercial authoring gap for mixed output shows. Gap 007. |
| RAM Studio deploy | `/api/studio-timeline` supports commissioning timeline with PIXEL + AUDIO:NODE, no autoStart | `web/showduino-studio/README.md` | `docs/studio/README.md` | S3 API → P4 | YES — CODE | HARDWARE REQUIRED | Separate from persistent production storage. |
| Pixels | P4 GPIO23 segmented engine supports up to 16 segment slots and shared FX vocabulary | `docs/audio-pixel-engine.md` | `docs/command-protocol.md` | `src/ShowPixels.*`, `protocol/showduino_pixel_fx.h` | YES — CODE | HARDWARE REQUIRED | Current default count 100; configured max capacity differs from installed line. |
| Pixels | Production-file PIXEL cues are not complete in persistent v1 | `docs/audio-pixel-engine.md` | `docs/production-storage.md` | parser/runtime integration | NO — PLANNED | NO | Do not present commissioning controls as complete show authoring. |
| Audio Node | Attraction/programme WAV playback is specialist Audio Node | `docs/audio-node.md` | `docs/audio-pixel-engine.md` | `firmware/audio-node-esp32/` | YES — CODE | HARDWARE REQUIRED | WAV current; MP3/Ogg/FLAC planned. |
| Audio Node | Emergency stops/mutes attraction audio; clear returns safe idle/no resume | `docs/audio-node.md` | Audio Node protocol/tests | Audio Node firmware | YES — CODE | HARDWARE REQUIRED | P4 reconciles Node Emergency state. |
| Node ownership | Nodes may be STANDALONE or SHOW_CONTROLLED; P4 GRANT creates Showduino ownership | `docs/standalone-node-architecture.md` | node firmware | ownership state machines | YES — CODE | HARDWARE REQUIRED | Live ON/FX/PLAY is not persisted as boot state. |
| Lamp Node | S3 Lamp is active; physical pins confirmed; replaces earlier C3/Relay direction | `docs/repository-status.md` | `docs/standalone-node-architecture.md` | `firmware/s3-lamp-node/` | YES — CODE | PARTIAL | Some sensor details remain unconfirmed. |
| Pixel Node | C3 Pixel Node firmware/routing exists | `docs/repository-status.md` | `docs/command-protocol.md` | `firmware/c3-pixel-node/` | YES — CODE | NO — HARDWARE REQUIRED | Treat as RC accessory, not fully signed-off commercial node. |
| Emergency Node | Wireless Emergency Node is assert-only and cannot clear P4 emergency | `docs/emergency-node.md` | `docs/command-protocol.md` | `firmware/c3-emergency-node/` | YES — CODE | NO — GPIO UNCONFIRMED | Momentary pushbutton + OLED; separate from P4 main button. |
| Emergency Node | Up to 8 logical Emergency stations; offline is safety-node warning, not automatic global emergency | `docs/emergency-node.md` | protocol header | Emergency Node link | YES — CODE | NO — HARDWARE REQUIRED | One-at-a-time update policy. |
| MOSFET Node | MOSFET Node is planned, not implemented | `docs/repository-status.md` | `docs/standalone-node-architecture.md` | none active | NO — PLANNED | NO | Not normal manual feature. |
| Relay Node | Old Relay Node is retired/legacy | `docs/repository-status.md` | `docs/command-protocol.md` | legacy firmware remains | LEGACY | n/a | Must not be advertised as current. |
| DMX | DMX production work is parked/out of scope | `docs/command-protocol.md` | README/architecture | no current production path | NO — PLANNED/PARKED | n/a | Do not expose as shipping production feature. |
| E1.31 | P4 has isolated test/observation foundation, not normal production lighting input | `docs/command-protocol.md` | `Network.js` | P4 E1.31 receiver | PARTIAL / TEST | HARDWARE REQUIRED | Network page diagnostics should not be mistaken for production support. |
| Diagnostics | Director has DIAGNOSTICS and SYSTEM LOGS pages; browser System page exposes P4/Comms health/logs | `DisplayPages.h` | `web/showduino-studio/README.md` | Director UI + Web API | YES — CODE | HARDWARE REQUIRED | Operator troubleshooting should start with status, not USB serial. |
| Updates | Only Communications S3 self-OTA is implemented in Phase 2A | `docs/system-updates.md` | `docs/comms-ota.md` | `src/update/CommsOta.cpp` | YES — CODE | HARDWARE REQUIRED | P4/Director/nodes remain USB-update components. |
| Updates | Comms OTA requires P4 alive, no running show, no global Emergency, maintenance authorisation, operator confirmation | `docs/comms-ota.md` | update manager protocol | Comms OTA + P4 maintenance gate | YES — CODE | HARDWARE REQUIRED | Internet is for download only; not a show-runtime dependency. |
| Updates | Comms OTA has SHA-256, inactive slot, health gate and software rollback | `docs/comms-ota.md` | OTA source | `CommsOta.cpp` | YES — CODE | HARDWARE REQUIRED | Manifest is not cryptographically signed; HTTPS uses `setInsecure()`. Do not overstate authenticity. |
| Storage | P4 SD is persistent backbone but not safety backbone | `docs/production-storage.md` | `docs/p4-sd-storage.md` | StageStorage/ProductionStore | YES — ARCH/CODE | HARDWARE REQUIRED | Loaded RAM timeline can continue if SD removed; streamed media may fault separately. |
| Plug-in bus | Unknown I²C device does not crash Stage; role is not inferred from chip alone | `docs/plugin-bus.md` | plugin source | `src/plugin/` | YES — CODE | HARDWARE REQUIRED | Short 3.3 V I²C commissioning bus, not industrial fieldbus. |
| Troubleshooting | P4 offline and Internet offline are separate fault domains | constitution/network docs | WebUI status model | P4/Comms health | YES — CODE/ARCH | HARDWARE REQUIRED | Manual fault tree preserves this distinction. |
| Pre-opening checks | Physical acceptance and emergency tests must be venue/operator routine | `docs/physical-test-checklist.md` | safety code/docs | multiple | DOCUMENTED | NOT YET RELEASE-SIGNED | Commercial checklist adapts these without developer flashing/serial tasks. |

---

## Sources explicitly reviewed in this pass

At minimum, this manual pass inspected or searched the active versions of:

- `README.md`
- `docs/constitution.md`
- `docs/architecture.md`
- `docs/SHOWDUINO_V1_ARCHITECTURE_REVIEW.md` (repository tree/search context)
- `docs/final-hardware-architecture.md`
- `docs/hardware-pinout.md`
- `docs/repository-status.md`
- `docs/audio-pixel-engine.md`
- `docs/audio-node.md`
- `docs/emergency-node.md`
- `docs/command-protocol.md`
- `docs/comms-ota.md`
- `docs/system-updates.md`
- `docs/production-storage.md`
- `docs/studio/README.md`
- `docs/plugin-bus.md`
- `docs/standalone-node-architecture.md`
- `docs/physical-test-checklist.md`
- `docs/v1-feature-matrix.md`
- `protocol/showduino_version.h` and protocol search results
- current P4, Comms S3 and Director firmware
- current browser WebUI source, including Network page and active page map
- recent `main` history affecting P4, Comms, Studio, networking, emergency, node reconciliation and updates.

## Baseline acquisition note

This pass was performed against the remote GitHub repository through the connected GitHub service. The execution container could not resolve `github.com`, so a local clone/pull and local `git status` could not be completed. To avoid touching unrelated work, all documentation changes were isolated on branch `docs/commercial-user-manual-2026-09-16`, created directly from the exact `main` starting SHA above. No firmware files were modified.
