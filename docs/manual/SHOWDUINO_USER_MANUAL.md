# SHOWDUINO V1 — User Manual

**Edition:** 1.0-RC, source-based rewrite of 19 November 2026  
**Product source:** `sumkindafreak/new_showduino` `main` at [`41c7c71ac617abdf9feeedf2064e6b5a9f31cc15`](https://github.com/sumkindafreak/new_showduino/tree/41c7c71ac617abdf9feeedf2064e6b5a9f31cc15)  
**Product version:** `1.0.0-rc.1`, protocol 1.x shared headers, portable Studio package **SHDO v2**  
**Readership:** operators, owners, system installers and supervising technicians  
**Release state:** **source-reviewed release candidate; hardware release acceptance is outstanding**

> **Read this first:** This is a guide to the **reviewed code**, not certification that the owner's flashed local firmware matches GitHub. After [source reconciliation PR #72](https://github.com/sumkindafreak/new_showduino/pull/72) was merged on 18 November, the earlier “main contains only generated Comms assets” status became historical. [Documentation PR #73](https://github.com/sumkindafreak/new_showduino/pull/73) separately records that source/physical distinction. The working local 22-second show and Emergency-clear fix are owner-reported successes; they do **not** yet establish two logged complete runs or a signed Emergency/no-auto-resume test. Preserve the known-good bench copy and use the [acceptance checklist](SHOWDUINO_PRE_OPENING_CHECKLIST.md) before venue operation.

## Contents

1. [Purpose and roles](#1-purpose-and-roles)
2. [Safety and permitted use](#2-safety-and-permitted-use)
3. [Hardware and physical connections](#3-hardware-and-physical-connections)
4. [Boot and readiness](#4-boot-and-readiness)
5. [Director touchscreen](#5-director-touchscreen)
6. [Network and browser surfaces](#6-network-and-browser-surfaces)
7. [Productions, cues and Studio](#7-productions-cues-and-studio)
8. [Running and stopping a production](#running-and-stopping-a-production)
9. [Emergency and Locate](#9-emergency-and-locate)
10. [Audio and lighting](#10-audio-and-lighting)
11. [Specialist nodes and outputs](#11-specialist-nodes-and-outputs)
12. [Storage, backups and updates](#12-storage-backups-and-updates)
13. [Diagnostics and fault recovery](#13-diagnostics-and-fault-recovery)
14. [Technical/service reference](#14-technicalservice-reference)
15. [Installation acceptance and limitations](#15-installation-acceptance-and-limitations)

## 1. Purpose and roles

Showduino is a modular show-control system for immersive experiences, attractions, practical effects and audio/lighting cues. It is designed to operate **one coordinated show** with one authoritative engine and multiple specialist nodes.

| Component | Hardware in reviewed build | Owner of |
| --- | --- | --- |
| **Show Engine (P4)** | Waveshare ESP32-P4 development board | Show state, timeline scheduling, production load, Emergency authority, node/outputs state as known by the system |
| **Communications Engine** | Dedicated ESP32-S3 | ESP-NOW, UART bridge, local Wi-Fi access point, browser Console, protocol proxy |
| **Director** | JC8048W550C ESP32-S3 800×480 GT911 touch | Operator requests, status, diagnostics and show supervision |
| **Audio Node** | ESP32-A1S ES8388 | Local show/programme audio and its attached pixel line |
| **Lamp, Pixel, MOSFET and Emergency Nodes** | Specialist S3/C3/ESP32 boards | Physical actions, local effects and report-back under P4 authority |

**Constitutional rule:** *The Show Engine decides. Communications transports. Director commands and displays. Nodes act.*

A loaded P4 production runs locally. It does **not** require a browser, internet, Director or venue router to remain open. Loss of a client link and loss of the Show Engine are different failures.

The older C3/SUE Comms architecture, external Mega executor and Relay Node sources are historical. The P4 onboard C6 is unused/reserved. Future DMX and other interfaces are **not** prerequisites for the present non-DMX system.

## 2. Safety and permitted use

**Showduino's Emergency function is a theatrical-control safeguard, not a certified fire alarm, evacuation device, SIL/PL controller or machinery E-stop.** Maintain all independently required venue safety systems and appropriate electrical protection. The operator must have a means to isolate dangerous loads independently.

- For initial commissioning, disconnect smoke, motion, high-power, mains-switched and other hazardous loads. Use visible, low-energy test outputs.
- Avoid exposed wiring, unfused 5 V strips, overcurrent, inadequate grounding, liquids near electronics or unverified power supplies.
- A **P4 Emergency** overrides entertainment effects, timeline actions and display controls. The main physical button can **assert** Emergency; it cannot clear it.
- An Emergency **clear** requires a deliberate Director/P4 process and **never automatically resumes** the show. Do not restart until the physical cause is resolved.
- Do not claim Emergency latch persistence through complete loss of P4 power; the current boot behaviour is not proved to restore a previously latched state after power removal.
- A wireless Emergency Node can assert P4 Emergency but does **not** replace an independent hardwired safety chain.
- For venue operation use the [Pre-Opening Checklist](SHOWDUINO_PRE_OPENING_CHECKLIST.md) and locally approved procedures, not source-code tests alone.

## 3. Hardware and physical connections

The three central control boards are the **P4**, **Comms S3** and **Director**. The Audio Node is a separate specialist device, not another P4.

### P4/Comms serial link

| Connection | Pin |
| --- | --- |
| P4 RX from Comms | P4 GPIO4 ← S3 GPIO17 TX |
| P4 TX to Comms | P4 GPIO5 → S3 GPIO18 RX |
| Both boards | Common ground, **115200 baud, 8N1** |

Do not connect 5 V signals directly to 3.3 V logic, reverse TX/RX or reuse the P4 onboard C6 SDIO pins.

### Principal P4 resources

| P4 pin / resource | Purpose |
| --- | --- |
| GPIO25, input pull-up | Main momentary Emergency to ground, active LOW, debounce |
| GPIO23 | Theatrical segmented **Show Pixel** line |
| GPIO24 | Separate Emergency/designated-signage pixel output |
| I²C GPIO7 SDA, GPIO8 SCL | Shared plugin/onboard codec bus, **3.3 V** |
| PCM5102A BCLK21 / WS20 / DOUT22 | Separate local ambience DAC |
| Onboard ES8311 | Showduino system and safety sounds, **not** attraction audio |
| Onboard microSD (SDMMC) | Persistent productions, configuration, assets and logs |

The Director uses an 800×480 RGB display and **GT911** touch. Its backlight timeout does not switch off the MCU. Avoid treating similarly named ESP32 LCD boards as pin-compatible; use the exact [Director BoardConfig](../../firmware/director-esp32-8048s050/ShowduinoDirector8048S050/BoardConfig.h).

**Fixed pixels are not programmable line defaults:** Audio status indicator = 1, Director ambient = 2, MOSFET identifier = 4, a standard fitted lamp Jewel = 7. A new programmable show line defaults to **at least 10** where the device limit permits; valid larger saved counts are retained.

The [Build and Parts Register](SHOWDUINO_BUILD_AND_PARTS_REGISTER.md) is the place to record every fitted PCB revision, PSU, wire, pixel count and actual acceptance measurement. Do not substitute a generic product photo for your installed module.

## 4. Boot and readiness

1. Confirm power, accessible Emergency button, correct removable media and safe disconnected loads.
2. Bring up **P4 → Comms S3 → Director → required specialist nodes** for a controlled RC test.
3. Confirm P4, UART transport, ESP-NOW links and each required node independently. A connected Director or browser alone is **not** a ready P4.
4. Confirm the correct production has been **loaded** and no Emergency, storage, node or output fault blocks the next action.
5. Rehearse with non-hazardous loads, capture logs and record node identities and firmware hashes.

A show already running on the P4 is **not automatically cancelled** because the Director touchscreen sleeps or the website disconnects. Do not use power cycling to clear a show-related fault without inspecting the P4.

## 5. Director touchscreen

The [Director Operator Guide](SHOWDUINO_DIRECTOR_OPERATOR_GUIDE.md) is the step-by-step reference for the reviewed active pages: **HOME, PRODUCTIONS, SHOW DETAILS, LIVE, CUE LIBRARY, NODES, AUDIO NODE, LAMP NODE, OUTPUTS, DIAGNOSTICS, SETTINGS, AUDIO, SYSTEM LOGS** and context-dependent system screens.

- **HOME:** overview and navigation. Its RUN action depends on reported link/readiness.
- **PRODUCTIONS:** select a saved production and ask the P4 to load it.
- **LIVE:** observe authoritative runtime, requested actions, progress and fault/complete result.
- **CUE LIBRARY:** a **native monitor** of active production cues; FOLLOW/BROWSE and deliberate START SCENE, not a SHDO editor. The older “tile disabled” instruction is obsolete in reviewed `main`.
- **OUTPUTS:** MOSFET/P4 I/O/plugin desk; separate displayed Actual from requested Set. APPLY is not a guarantee of physical switching.
- **AUDIO:** P4's system sound; the Audio Node is a different device.
- **SETTINGS:** backlight/brightness and saved five-point touch calibration where implemented.
- **Emergency screen:** the **only normal operator clearing workflow**, with P4 validation and deliberate confirmation.

The OS2 shell source tree is not the active default (currently `SHOWDUINO_OS2_SHELL=0`). Screen illustrations may be examples; verify physical UI labels against the installed build.

## 6. Network and browser surfaces

The Comms S3 hosts a **Showduino** SoftAP and the **System Console**. On default bench networking the browser entry point is often `http://192.168.4.1/`; use the actual configured address and credential for your installation. Do not reuse the source development password at a public venue.

The Comms **Network** page offers Wi-Fi **Scan, Connect, Disconnect, Forget, AP only**. Home/venue Wi-Fi association may change the ESP-NOW operating channel through the Comms gateway; do not force a competing radio channel on a node. External internet or GitHub availability is **not** a P4-ready signal.

There are three different browser/editor concepts:

1. **Comms System Console:** local Home/Productions/Live/Outputs/Devices/Network/System/Settings and P4 API proxy.
2. **Embedded `/studio/` snapshot:** a bundled edition of Studio; may lag the website's newest authoring code.
3. **Website Studio:** richer editing and SHDO export; independent release and compatibility boundary.

If the browser loads while the P4 is offline, controls must remain unavailable. The browser is not a substitute for the Director/P4 Emergency-clear handshake. A stale text label about a “3-second hold” in current `Live.js` is erroneous; never follow it.

## 7. Productions, cues and Studio

A **production** is the named show. A **scene** is a creative or timeline subdivision, not a second runtime authority. The P4 executes the ordered cues in the loaded production.

### Creating a new production

1. Open **Website Studio**, create/open a production and enter its Timeline.
2. Add Audio, Pixel, MOSFET or other compatible lanes; use **Cue Library** presets as editable starting points.
3. In the Inspector, assign actual logical node IDs and verified media/segment/output references. Check storage **by route**: `audio-node` uses the Audio Node SD; `p4-ambience` uses the P4 SD directory `/showduino/audio/ambience/`. Example media names are not automatically uploaded.
4. Preview for timing and run Project Check. **This is authoring validation only.**
5. Export an **SHDO v2** `.shdo` package; check the compiler compatibility of the **installed** P4 before deploying.
6. In the **Comms System Console → Productions → Persist SHDO v2 to P4 SD**, choose the exported file and press **Persist to SD**. Wait for the P4 success/rejection. Persist does **not** auto-load or auto-start.
7. Separately **Load**, confirm readiness and **Start** in a controlled test.

The P4 **compiles** supported SHDO cues to persistent runtime format v1 (rather than interpreting arbitrary authoring JSON). The canonical on-SD paths are `/showduino/productions/<id>/manifest.json` and `timeline.json`. P4 format-v1 supports typed bounded commands; see [production storage](../production-storage.md) and [Source Matrix](SHOWDUINO_MANUAL_SOURCE_MATRIX.md).

The current `ProductionFormat.cpp` accepts bounded **ESTOP** pixel cues with the `ESTOP:NODE:PIXEL:` prefix and bounded **AMBIENCE** commands (including `AMBIENCE:STOP`). The [production-storage reference](../production-storage.md) has been reconciled with those parser rules; the earlier statement excluding ESTOP was historical. For `p4-ambience` audio clips, the SHDO compiler emits `AMBIENCE` playback/volume/stop cues that reference media on the **P4 SD**, not on the Audio Node. No authoring palette item, particularly legacy Relay/Trigger/generic FX/DMX, should be described as deployable without current compiler **and physical** verification.

### RAM commissioning is not persistent storage

The Console's `/api/studio-timeline` RAM upload is a separate commissioning path (Pixel/Audio Node scope). It must not be represented as a saved SD production. A timed browser preview, a RAM test and a persisted compiled production are distinct.

### Why a deployment can fail

Likely boundaries include wrong SHDO version, unavailable node route, missing asset, invalid cue command, range/size limits, wrong timeline order, storage failure or an active Emergency. Preserve the actual rejection. Avoid upgrading/merging an untested firmware branch merely to accept a previously rejected package.

## Running and stopping a production

| Request | What it means | What to verify |
| --- | --- | --- |
| **Load** | P4 selects/loads an SD production | Confirm loaded ID/state from P4 |
| **Start** | P4 begins the loaded timeline if allowed | RUNNING state, cue activity and real output |
| **Pause** | P4 pauses the runtime | PAUSED state and expected output policy |
| **Resume** | Deliberate request from an allowed paused state | P4 confirmation; Emergency must be clear |
| **Stop** | P4 ends the current run | Safe output result and stopped state |
| **Unload** | Remove loaded production when allowed | P4 accepts only in an allowed state |
| **Complete** | P4 finished the scheduled timeline | Authoritative completion, not merely UI animation |

A production may be stopped or its runtime state lost for operational reasons. Never promise automatic recovery/rejoin without an explicit feature-specific test. A successful SHDO persist **never** initiates playback by itself.

For acceptance, execute the **same known-good non-DMX production twice** through P4-confirmed LOAD → START → COMPLETE, then test Emergency interruption on a separate attempt. Do not use an unverified new show to close the original benchmark.

## 9. Emergency and Locate

### Physical main button

P4 GPIO25 is an internally pulled-up **active-LOW** momentary input. Once a valid press is debounced, the **P4 latches Emergency** and overrides entertainment requests. **Releasing** the button does **not** clear the latch.

An uninterrupted **8-second hold** additionally requests **LOCATE**. The Director wakes, marks Locate and flashes its fitted ambient pixels; the first deliberate touch acknowledges Locate without activating an underlying button. **Locate never clears Emergency.**

### Safe clear procedure

1. Protect people, inspect the installation and remove the cause. Do not bypass independent venue controls.
2. **Release the physical button.** A still-asserted input blocks clearance.
3. On **Director Emergency**, press **CLEAR EMERGENCY**.
4. Await the P4 request acknowledgement. At **Clear Emergency Stop?** deliberately select **CONFIRM CLEAR** or cancel.
5. Wait for P4 authoritative clear. If the request rejects, hangs or times out, **stop operations** and diagnose with P4/Comms/Director logs.
6. Verify safe outputs and **no automatic restart**; the operator must initiate a separate authorised run/resume after review.

A further Emergency assertion invalidates stale pending confirmation. The audio and lamp nodes reconcile reported Emergency state with P4 authority; this is not a claim that the full node matrix has passed physical acceptance.

### Emergency lighting and sound

The P4 theatrical show line and designated signage drive bright white in Emergency; other configured pixel-capable nodes have their own Emergency-white rules. Emergency overrides show brightness and effects. Clearing changes signage back to its normal indication and leaves show output in a safe state rather than automatically returning to the interrupted scene. Show/programme audio must not unexpectedly resume.

The P4 plays **system** Emergency sound where hardware/media permit, but safety must not depend on a WAV file playing. Independent venue safety systems remain mandatory.

### Critical limitations

The P4 software latch is **not proved to persist across complete power removal** after button release. Do not assert otherwise. Wireless Emergency Nodes are **assert-only** and not certified substitutes for hardwired stops. A stuck **AWAITING STAGE** screen, no P4 confirmation or unexpected Director reset is a **failure**, not clearance.

## 10. Audio and lighting

### P4 system sound versus show audio

The P4 onboard **ES8311** and associated amplifier provide Showduino system sounds such as boot, Emergency, error and completion. Stored system sounds live under `/showduino/audio/system/` (e.g. `boot.wav`, `emergency.wav`).

The **Audio Node** (ESP32-A1S/ES8388) is for programme audio: dialogue, music, ambience, sound effects and show-timed cues. Its media lives on its **own SD card**. It does **not** fall back to the P4 system speaker if offline.

The P4 may also have an external **PCM5102A local ambient output** on GPIO20/21/22; this is separate from both the P4 system codec and the specialist programme Audio Node.

### Pixel lines

- **P4 GPIO23:** normal segmented Show Pixel line for theatrical effects.
- **P4 GPIO24:** separately safety-owned Emergency/designated signage; **not** a free show effect output.
- **Audio Node GPIO22:** programme-associated configurable pixel line on the **same Audio Node identity**.
- **C3 Pixel Node:** remote theatrical output; P4 still schedules its cues.
- **Wireless Emergency C3 GPIO2:** pixel capability on the **same ESTOP identity**, not a fake second peer.
- **Lamp fitted Jewel/Director ambient/MOSFET identifier:** fixed assembly indicators, not lines subject to the default 10-programmable-LED rule.

New programmable lines default to **10** valid pixels; correctly configured larger counts remain. Use a suitable 5 V supply, common ground and proper signal level-shifting/protection for installed strips. Emergency bright white must have adequate supply current.

## 11. Specialist nodes and outputs

### Audio Node

Ai-Thinker ESP32 Audio Kit V2.2 **A161/ES8388**, not AC101. Commission the node's local SD, output volume, audio route, programme files, status pixel and GPIO22 show line. P4 controls playback and Emergency; node-side local modes do not create separate P4 show authority. See [Audio Node](../audio-node.md).

### S3 Lamp Node and proposed simplified C3 Lamp

The active repo includes an **S3 carbide-lamp simulator** with a seven-LED Jewel, local effects/sensors and Showduino-managed mode. The **simplified single-button ESP32-C3 hurricane-lantern design** is separately specified; it is **not** the same as installed/signed-off S3 firmware or proof a ready simplified C3 firmware is deployed. See [S3 Lamp](../s3-lamp-node.md) and [C3 Lamp design status](../c3-lamp-node-simplified-integration.md).

### C3 Pixel Node

Remote pixel effects are output by its local hardware. Confirm node ID, actual GPIO mapping, LED count, supply and Emergency behaviour, and physical results before use. See [C3 Pixel](../c3-pixel-node.md).

### Wireless Emergency + Pixel C3

Extra station asserts the **global** P4 Emergency. It **cannot clear** the latch. Its pixel line is tied to the same logical ESTOP node. Confirm physical input wiring and radio health; do not treat wireless as independent certified Emergency infrastructure. See [Emergency Node](../emergency-node.md).

### Four-channel MOSFET Node

Provides **low-voltage DC** output control and reported Actual values. Director **Set** is a desired value, not proof of switched current. APPLY requests a change, while OFF/ALL OFF are safe-off requests with special gates. Never use this as a mains switching specification. Recheck the earlier Director-reconnect drop report on the installed board before release. See [Outputs/MOSFET](../director-outputs-mosfet.md).

### Plug-in Bus and legacy options

The P4's **3.3 V I²C** bus uses GPIO7/8 and shares resources with its onboard codec. Only connect address/power-compatible approved devices; do not infer completion of a new plugin from discovery alone. Historical Relay code and DMX experiments are **not** present production commissioning steps.

## 12. Storage, backups and updates

The P4 SD is the persistent backbone (productions, configuration, logs, assets), **not the Emergency safety backbone**. A timeline already loaded into RAM may continue without SD; media streaming or future loads may fail. Back up the full `/showduino/` card tree and every node's local media before changes.

The Comms firmware embeds the local System Console. Changing website files alone does **not** update a flashed Comms bundle; developers rebuild it separately. Operators should not hand-edit generated WebUI headers.

**Updates:** Comms **self-OTA** is the documented limited update path subject to installation-specific physical proof. This is **not** a system-wide OTA. P4, Director and most specialist nodes remain USB-flashed in the reviewed release candidate. Never update firmware during a live show or Emergency, and never replace a working local build without backup, compatibility check, controlled test and rollback plan.

No GitHub PR, clean host build, green CI or release manifest proves the binaries currently fitted in an attraction. See [System Updates](../system-updates.md) and [Comms OTA](../comms-ota.md).

## 13. Diagnostics and fault recovery

| Symptom | Meaning and first action |
| --- | --- |
| Director **CONNECTION LOST** | Director/Comms link may be down; check power and transport. **Do not assume P4 stopped.** |
| Browser works, **P4 OFFLINE** | Comms serves UI independently; inspect P4 UART, boot and state. |
| Production does not load/start | Look for missing card, rejected format, invalid cue, unready node or Emergency; read P4 response. |
| **NO SD CARD** | No reliable persistent production access; inspect card/power safely, don't assume auto-load. |
| Venue Wi-Fi down | Comms SoftAP may still work; external Internet is optional. |
| Audio silent | Verify Audio Node online, card, file, level, amplifier, Emergency and node ID; P4 system sound is distinct. |
| Pixels bright white | **Treat as possible Emergency**; inspect P4 authoritative latch **before** attempting effects. |
| Show pixels black after clear | Safe post-clear behaviour; do not replay the previous effect automatically. |
| MOSFET Actual differs from Set | Actual is node feedback; isolate load, diagnose and retest. |
| Emergency clear stuck/rejected | Confirm physical release and P4 report; capture logs. No reset workaround. |
| Cue Library screen stale | Source support does not prove hardware fix; log exact flashed build and steps. |
| Node reports standalone/controlled | Observe P4 ownership and local permission rules; do not fight show authority. |

For a reproducible fault: record the time, production ID, firmware hashes and **P4 → Comms → Director → Node** states; log the exact last accepted request and result. For dangerous behaviour, disconnect the effect and stop venue operation first.

## 14. Technical/service reference

**Current source roots:**

- `firmware/stage-engine-p4/ShowduinoStageEngineP4/` — P4 authority, Emergency, production parser and runtime.
- `firmware/s3-comms-controller/ShowduinoS3CommsController/` — S3 transport, AP, WebUI and OTA.
- `firmware/director-esp32-8048s050/ShowduinoDirector8048S050/` — Director pages, buttons, touch and render logic.
- `firmware/audio-node-esp32-a1s/`, `firmware/s3-lamp-node/`, `firmware/c3-pixel-node/`, `firmware/c3-emergency-node/`, `firmware/mosfet-node-esp32/` — current specialist implementations.
- `protocol/` — shared versioning, SHDO, messages and safety/pixel contracts.
- `web/showduino-studio/` — editable S3 Console frontend.
- `docs/` — architecture, hardware, production, system updates and node-specific references.

Use [Source Matrix](SHOWDUINO_MANUAL_SOURCE_MATRIX.md) for claim-by-claim evidence. Historical documents remain in Git history. A published path or function is **source evidence**, not a pinout measured on the installed enclosure.

## 15. Installation acceptance and limitations

**Evidence known at 19 November 2026:**

- Owner-reported **first combined audio/visual physical show** on 29 October 2026.
- Owner-reported local **22-second full-node production running**, and local Emergency-clear correction after earlier failures.
- Merged source reconciliation [#72](https://github.com/sumkindafreak/new_showduino/pull/72), with host build/test evidence; not a physical pass.
- **Not yet recorded/signed:** two consecutive P4-confirmed complete productions; a full mid-show Emergency → all safe outputs → physical release → Director/P4 deliberate clear → **no auto-resume** pass.
- Director freeze/Cue Library navigation and MOSFET Director-reconnect behaviour require physical retest as relevant.
- Unverified enclosure identification, node fault recovery, media readiness and full site safety compliance must stay **open** until documented.

**Release rule:** An operator procedure may be published as source-reviewed without claiming the finished installation is release-ready. The [Verification Gaps](SHOWDUINO_MANUAL_GAPS.md) are part of this manual, not optional fine print.

### Glossary

**P4:** authoritative Show Engine. **Comms:** dedicated S3 transport and web gateway. **Director:** operator touchscreen. **Node:** specialist local actuator/player. **SHDO v2:** portable authoring document. **Format v1:** P4 runtime production representation. **Persist:** compile/store to SD (not load). **Load:** select production into P4 runtime (not start). **Emergency:** P4-controlled software latch. **Locate:** Director-finding indication while Emergency remains latched.

### Further reading

[Quick Start](SHOWDUINO_QUICK_START.md) · [Director Guide](SHOWDUINO_DIRECTOR_OPERATOR_GUIDE.md) · [Studio Cue Library](SHOWDUINO_STUDIO_CUE_LIBRARY_GUIDE.md) · [Pre-Opening Checklist](SHOWDUINO_PRE_OPENING_CHECKLIST.md) · [Parts Register](SHOWDUINO_BUILD_AND_PARTS_REGISTER.md) · [Source Matrix](SHOWDUINO_MANUAL_SOURCE_MATRIX.md) · [Verification Gaps](SHOWDUINO_MANUAL_GAPS.md).
