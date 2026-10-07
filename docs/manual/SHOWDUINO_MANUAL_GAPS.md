# Showduino Commercial Manual — Verification Gaps

**Manual baseline date:** 7 October 2026\
**Repository:** `sumkindafreak/new_showduino`\
**Baseline branch:** `main`\
**Baseline commit:** `526af84cc04d96b3f4d9a51bc3a0ef1470da0b69`\
**Previous commercial-manual baseline:** `9f304cdc65786c0e8d306987010a97eb5a3ad587` (16 September 2026)\
**Product baseline:** Showduino `1.0.0-rc.1`\
**Manual revision:** 0.3-RC

This document records anything that could not honestly be presented as finished commercial product behaviour during the repository-verified manual pass. It is deliberately conservative: uncertainty belongs here, not disguised as customer-facing fact.

The categories used are:

- **IMPLEMENTATION GAP** — approved or documented behaviour is not implemented at this baseline.
- **DOCUMENTATION GAP** — code appears to define behaviour but the product documentation is incomplete or contradictory.
- **UI/DOCUMENTATION MISMATCH** — operator wording or workflow differs between code/UI and documentation.
- **HARDWARE VALIDATION REQUIRED** — source exists but physical acceptance is not complete.
- **SAFETY PROCEDURE TO CONFIRM** — safety-related behaviour or operator procedure needs explicit acceptance.
- **VERSION INFORMATION REQUIRED** — component/release version sources do not agree.
- **PRODUCT DECISION REQUIRED** — behaviour needs an owner decision before it should be frozen into a commercial manual.

Status labels used in this revision:

- **OPEN**
- **PARTIALLY RESOLVED**
- **RESOLVED** (software) — may still carry HARDWARE VALIDATION REQUIRED
- **SUPERSEDED**

---

## GAP-001 — Main Emergency button 8-second Locate hold

**Status:** RESOLVED (software) — HARDWARE VALIDATION REQUIRED\
**Category:** IMPLEMENTED — HARDWARE ACCEPTANCE REQUIRED\
**Severity:** Release-blocking until physical bench sign-off\
**Current code:** `firmware/stage-engine-p4/ShowduinoStageEngineP4/src/EmergencyInput.cpp`, `BoardConfig.h`, `protocol/showduino_emergency_button.h`

### Approved product requirement

The momentary main-unit Emergency button must:

1. assert and latch Emergency immediately on button-down;
2. start a hold timer without delaying Emergency;
3. if the *same continuous press* reaches 8 seconds, additionally request Director Locate;
4. keep Emergency and Locate as independent states;
5. never clear Emergency from the physical button.

### Implementation status

Implemented in firmware:

- debounced press asserts Emergency immediately;
- `SHOWDUINO_ESTOP_LOCATE_HOLD_MS` (8000) on the same uninterrupted press requests `DIRECTOR:LOCATE` once;
- release never clears and does not accumulate hold time;
- the retired 8-press / 6-second Locate gesture is gone;
- the retired 3-second physical hold-to-clear gesture is gone.

Physical bench acceptance of this path is still required before commercial validation.

---

## GAP-002 — Director Locate presentation / first-touch acknowledgement

**Status:** RESOLVED (software) — HARDWARE VALIDATION REQUIRED\
**Category:** IMPLEMENTED — HARDWARE ACCEPTANCE REQUIRED\
**Current code:** `DirectorLocateScreen.*`, `DirectorAmbientPixels.cpp/.h`, `backlight.cpp`, `touch_lvgl.cpp`

Implemented in firmware:

- `DIRECTOR:LOCATE` wakes the display and forces backlight on;
- normal auto-off is temporarily suspended without rewriting saved settings;
- ambient white locator flashes with **no 15-second timeout**;
- **LOCATE ACTIVE** overlay states that Emergency remains active;
- the first deliberate touchscreen press acknowledges Locate only;
- that press/release cycle is consumed and cannot click an underlying control;
- Locate acknowledgement does not clear Emergency.

Hardware-test sleep/wake, touch consumption and emergency-latch retention before treating this as commercially validated.

---

## GAP-003 — Emergency clear is Director request → confirm (physical button never clears)

**Status:** RESOLVED (software) — HARDWARE VALIDATION REQUIRED\
**Category:** IMPLEMENTED — HARDWARE ACCEPTANCE REQUIRED\
**Current code:** P4 `EmergencyInput.cpp` / `ShowduinoStageEngineP4.ino`; Director `DirectorEmergencyScreen.cpp`, `DirectorEmergencyClearDialog.cpp`

Implemented production clear workflow:

- operator chooses **CLEAR EMERGENCY** on the Director Emergency screen;
- Director sends `EMERGENCY:CLEAR` (request);
- P4 rejects if not latched or if GPIO25 is still pressed;
- otherwise P4 creates a timed pending authorisation and sends `EMERGENCY:CLEAR_REQUEST`;
- Director shows **EMERGENCY CLEARANCE REQUESTED** / **Clear Emergency Stop?**;
- `CONFIRM CLEAR` sends `EMERGENCY:CLEAR_CONFIRM`;
- P4 validates again (latched, pending, button released, no newer assertion) then clears;
- a new physical/wireless Emergency assertion invalidates any stale pending confirm;
- clear does not resume the show.

Physical acceptance of this handshake is still required. Do not mark commercially validated until the bench checklist is signed.

---

## GAP-004 — Emergency latch is not proven to persist through full power loss/reboot

**Status:** OPEN\
**Category:** SAFETY PROCEDURE TO CONFIRM / PRODUCT DECISION REQUIRED\
**Current code:** P4 `ShowduinoStageEngineP4.ino`, `EmergencyInput.cpp`

The authoritative P4 variable `emergencyLocked` is initialised `false` at boot. No persistent restore of that latch was found in the active P4 code during this pass.

A physically asserted button at boot should be detected again by the input service after debounce, but that is not the same thing as persisting a previously latched momentary-button emergency through complete power removal after the button has been released.

### Required resolution

Decide the required commercial behaviour for reboot/power loss during a latched Emergency and physically validate it. Until then the manual must not state that the latch survives full power removal.

---

## GAP-005 — Emergency/clear physical acceptance remains incomplete

**Status:** OPEN\
**Category:** HARDWARE VALIDATION REQUIRED / SAFETY PROCEDURE TO CONFIRM\
**Primary evidence:** `docs/physical-test-checklist.md`, `docs/v1-feature-matrix.md`

The repository explicitly states that source inspection is not physical acceptance. Release-blocking checks include:

- physical GPIO25 latch;
- output emergency states;
- no automatic show resume;
- Director clear workflow;
- GPIO23/GPIO24 emergency-white behaviour;
- node emergency behaviour (including Audio GPIO22 and ESTOP GPIO2 pixel white/black).

The dated physical-acceptance record states that no attached Showduino hardware was available on the validation host and the physical items remain not run/blocked.

### Required resolution

Complete and sign off the physical emergency matrix before `v1.0.0` and before a final production manual revision.

---

## GAP-006 — Full first-owner authoring journey is not yet one finished product workflow

**Status:** PARTIALLY RESOLVED\
**Category:** IMPLEMENTATION GAP / UI/DOCUMENTATION MISMATCH\
**Primary evidence:** `docs/studio/README.md`, `web/showduino-studio/README.md`, `web/studio-v4-overlay/`

The intended commercial journey is conceptually:

`Create attraction/show → create scene → add devices → build timeline → validate → deploy → play`.

At this baseline, two different surfaces share the Studio name:

1. the S3-hosted **system console / commissioning WebUI**, which is active;
2. the broader **Studio V4 / SHDO v2 authoring** snapshot, which now includes substantial Pixel authoring (P4, standalone Pixel Nodes, Audio Node pixels), audio cues and package/export work.

What remains unfinished for a single commercial “first owner” journey:

- persistent P4 production format still cannot store the full theatrical cue set (see GAP-007);
- public HTTPS Studio may still need export when browsers cannot reach local hardware;
- Emergency Node pixel outputs are implemented in firmware/SHDO (`estop-node-pixels`) but Studio inventory surfacing may still lag (“showduino.com follow-up” note in `docs/emergency-node.md`).

### Required resolution

Complete and freeze the customer-facing authoring/deployment workflow, then capture its exact current labels/screens in the manual. Until then, the main manual distinguishes live commissioning/deployment capability from the unfinished persistent mixed-device path.

---

## GAP-007 — Persistent production format cannot yet represent the full theatrical feature set

**Status:** PARTIALLY RESOLVED\
**Category:** IMPLEMENTATION GAP\
**Primary evidence:** `ProductionFormat.cpp`, `docs/production-storage.md`, `docs/audio-pixel-engine.md`

P4 persistent production format v1 now accepts bounded `PIXEL`, `AUDIO`, `LAMP`, `MOSFET`, `TEST` and `LOG` cues (`ProductionFormat.cpp`). SHDO deploy can compile into that store.

Still incomplete relative to the full commercial theatrical set:

- no dedicated ESTOP cue type on load; `PIXEL` cues must use the `PIXEL:` command prefix (so `ESTOP:NODE:PIXEL:` is not a format-v1 PIXEL command as stored);
- tight command-length and validation bounds;
- Studio inventory/authoring for every pixel-capable peer is not equally complete (see GAP-021);
- physical acceptance of mixed-device persisted shows remains open.

Earlier commercial-manual wording that said “TEST/LOG only” was stale relative to current parser behaviour and has been corrected in manual revision 0.3-RC follow-up.

### Required resolution

Complete ESTOP/other missing persist routes, widen the theatrical cue model where product-approved, and sign off mixed-device load/run on hardware before describing full persistent theatrical shows as finished.

---

## GAP-008 — “Studio” naming currently covers two materially different surfaces

**Status:** OPEN\
**Category:** PRODUCT DECISION REQUIRED / DOCUMENTATION GAP\
**Primary evidence:** `docs/studio/README.md`, `web/showduino-studio/README.md`

“Studio” currently refers both to the S3-hosted system/commissioning console and to the richer SHDO v2 authoring experience/blueprint. This can confuse owners reading a commercial manual.

Manual 0.3-RC uses **System Console** vs **Studio** wording where practical, but product naming is not frozen in firmware/UI chrome.

### Required resolution

Freeze customer-facing names for:

- the system/commissioning WebUI;
- the production authoring/timeline application;
- the `/studio/` embedded authoring snapshot.

Then use those names consistently in firmware, browser navigation and documentation.

---

## GAP-009 — Default SoftAP credential is a bench credential, not a finished venue provisioning story

**Status:** OPEN\
**Category:** PRODUCT DECISION REQUIRED / HARDWARE VALIDATION REQUIRED\
**Current source:** Comms `BoardConfig.h`

The current Communications S3 SoftAP is:

- SSID: `Showduino`
- WPA2 password: `showduino`

The code itself labels this as a documented **bench** secret and says it must be changed before a public venue. During this pass, an operator-facing workflow for changing the Showduino SoftAP password itself was not verified; the Network page configures optional home/venue STA credentials.

### Required resolution

Define production provisioning for the Showduino AP credential (unique-at-build, first-run change, managed setting, or another explicit policy) before public deployment guidance is finalised.

---

## GAP-010 — Network configuration is implemented but still awaiting bench acceptance

**Status:** OPEN\
**Category:** HARDWARE VALIDATION REQUIRED\
**Primary evidence:** `docs/v1-feature-matrix.md`, `docs/physical-test-checklist.md`

AP+STA behaviour, retained SoftAP, radio-channel following, Director/Audio/Lamp reconnect and non-channel-1 venue Wi-Fi operation are implemented foundations but remain on the V1 hardware acceptance list.

### Required resolution

Run the release-blocking radio/power-cycle matrix, especially Director stability while specialist nodes join and while venue Wi-Fi changes the operating channel.

---

## GAP-011 — Comms self-OTA exists, but system-wide OTA does not and physical OTA proof remains a gate

**Status:** PARTIALLY RESOLVED (docs) / OPEN (hardware proof)\
**Category:** HARDWARE VALIDATION REQUIRED / DOCUMENTATION GAP\
**Primary evidence:** `docs/comms-ota.md`, `docs/system-updates.md`, Comms OTA source

Current code supports **Communications Controller self-OTA only**, with HTTPS download, SHA-256 verification, inactive-slot write, health gate and software rollback. P4, Director and specialist nodes remain USB-update components at this phase.

Commercial manuals correctly describe Comms-only OTA. Physical proof of normal Comms OTA and failed-health rollback remains outstanding.

### Required resolution

Physically prove normal Comms OTA and failed-health rollback on the current hardware/firmware combination.

---

## GAP-012 — Release manifest component versions lag current `main`

**Status:** OPEN\
**Category:** VERSION INFORMATION REQUIRED\
**Sources:** `releases/showduino-1.0.0-rc.1.manifest.json`, current component `BoardConfig.h`

The committed RC release manifest still lags live source. At this documentation SHA:

- P4 `BoardConfig.h` reports `0.6.9` (updated here from source baseline 0.6.8);
- Comms `BoardConfig.h` reports `0.5.4`;
- Director `src/StorageConfig.h` reports `0.9.11-director`;
- Audio/Lamp/Pixel/Emergency/MOSFET versions are 0.4.3 / 0.4.3 / 0.1.1 / 0.3.1 / 0.1.3 (Lamp emergency playback retained). These are source labels, not verified installed binaries.

### Required resolution

Publish/update an authoritative release inventory that matches the exact binaries accepted for a manual revision. The user manual therefore uses product version `1.0.0-rc.1` plus the exact repository SHA as its strongest baseline identifier.

---

## GAP-013 — Director OS 2.0 source exists but is not the active baseline UI

**Status:** OPEN (unchanged product fact)\
**Category:** UI/DOCUMENTATION MISMATCH\
**Current source:** Director `BoardConfig.h`

`SHOWDUINO_OS2_SHELL` is `0` at the baseline SHA. The active Director is therefore the current `ShowduinoUi` page set, even though newer OS 2.0 app/service source exists in the tree.

### Required resolution

When the OS 2.0 shell becomes the shipping UI, revise the Director chapter and screenshots as one versioned manual change. Do not document disabled source as current behaviour.

---

## GAP-014 — Current Director page set and diagnostics are still changing

**Status:** PARTIALLY RESOLVED\
**Category:** DOCUMENTATION GAP / HARDWARE VALIDATION REQUIRED\
**Sources:** Director `DisplayPages.h`, `ShowduinoUi.h`, recent Git history

Active titles now include dedicated **AUDIO NODE** and **LAMP NODE** pages in addition to HOME / PRODUCTIONS / SHOW DETAILS / LIVE / NODES / DIAGNOSTICS / SETTINGS / AUDIO / SYSTEM LOGS. Manual 0.3-RC was updated to match those titles. A commercial screenshot set would still become stale quickly and remains outstanding.

### Required resolution

Freeze a release-candidate UI build, capture screenshots from that exact binary and add figure references to a later manual revision.

---

## GAP-015 — P4 local pixel hardware is implemented but not signed off

**Status:** OPEN\
**Category:** HARDWARE VALIDATION REQUIRED\
**Sources:** `docs/hardware-pinout.md`, `docs/audio-pixel-engine.md`, `docs/physical-test-checklist.md`

GPIO23 segmented Show Pixels and GPIO24 emergency/designated-signage pixels are implemented in source. The repository still marks the physical pixel paths as requiring hardware commissioning.

### Required resolution

Validate configured lengths, logic buffering, current capacity, all-white emergency load, segment behaviour, clear-to-blackout and signage grouping on production-equivalent wiring.

---

## GAP-016 — Specialist node maturity is mixed

**Status:** PARTIALLY RESOLVED (software maturation) / OPEN (hardware acceptance)\
**Category:** HARDWARE VALIDATION REQUIRED\
**Sources:** `docs/repository-status.md`, node-specific documentation, recent `main` commits

At this baseline:

- Audio Node — programme audio + full GPIO22 Pixel engine; hardware acceptance still required;
- S3 Lamp Node — active; Adafruit Audio FX UART WAV library; physical pins confirmed; some sensor details remain unconfirmed; Director Lamp page exists;
- C3 Pixel Node — active, hardware test required;
- C3 Emergency + Pixel Node — software implemented (momentary GPIO4, OLED, GPIO2 full Pixel); GPIO/physical acceptance incomplete;
- MOSFET Node — active RC firmware 0.1.3, powered-output routing/deploy and identifier pixels implemented; physical acceptance required;
- Relay Node — legacy/retired.

### Required resolution

Only ship/manualise a node as a normal supported accessory once its hardware acceptance is complete. The main manual labels current RC maturity and does not present MOSFET/legacy Relay as shipping features.

---

## GAP-017 — Wireless Emergency Nodes are not equivalent to the main momentary Emergency button

**Status:** PARTIALLY RESOLVED (docs) / OPEN (physical acceptance)\
**Category:** DOCUMENTATION GAP / SAFETY PROCEDURE TO CONFIRM\
**Source:** `docs/emergency-node.md`, `firmware/c3-emergency-node/`

The separate wireless Emergency Node design uses a **momentary pushbutton**, local software latch, OLED status, optional full Pixel Line on GPIO2, and **assert-only** wireless policy. Its physical control and clear-observation model differ from the main Showduino momentary GPIO25 button.

Commercial manuals now describe the accessory separately. Physical acceptance remains open.

### Required resolution

Keep accessory-specific instructions separate. Never copy the main-button Locate/clear gesture into the Emergency Node chapter unless its own firmware explicitly implements it.

---

## GAP-018 — Commercial enclosure connector labelling cannot be verified from firmware alone

**Status:** OPEN\
**Category:** HARDWARE VALIDATION REQUIRED / PRODUCT DECISION REQUIRED

The repository defines board-level pins and electrical expectations, but this pass did not identify a frozen commercial enclosure drawing, rear-panel connector schedule, fuse specification, PSU rating or production wiring diagram that maps those internal resources to final customer-accessible labels.

### Required resolution

When enclosure/loom/power design is frozen, add controlled hardware drawings and rated connector/power specifications. Until then, the manual's Hardware Tour remains functional rather than inventing panel labels.

---

## GAP-019 — No certification claims are supported by the repository

**Status:** OPEN (accuracy boundary — intentional)\
**Category:** PRODUCT DECISION REQUIRED

No evidence was found that Showduino is certified as a fire alarm, evacuation system, machinery E-stop, safety PLC, SIL-rated system or PL-rated system.

### Manual consequence

The commercial manual explicitly keeps venue/fire/machinery/life-safety compliance separate from Showduino show-control emergency behaviour. This is not a negative product claim; it is an accuracy boundary until formal certification exists.

---

## GAP-020 — Director touchscreen calibration is implemented but not hardware-accepted

**Status:** RESOLVED (software) — HARDWARE VALIDATION REQUIRED\
**Category:** IMPLEMENTED — HARDWARE ACCEPTANCE REQUIRED\
**Current code:** `TouchCalibrationMath.h`, `TouchCalibrationStore.cpp`, `DirectorTouchCalibrationScreen.*`, `touch_lvgl.cpp`

Five-point affine calibration, NVS persistence (`showduino_touch` / `cal`), factory fallback, Settings entry, USB `TOUCH:STATUS` / `TOUCH:RESET` / `TOUCH:CALIBRATE`, Emergency/Locate abort, and first-touch consumption while the wizard is open are in firmware.

Do not treat corner accuracy, reboot persistence, or overlay consumption as commercially validated until the physical checklist is signed.

---

## GAP-021 — Studio Emergency Node pixel inventory surfacing lags firmware/SHDO

**Status:** OPEN (new)\
**Category:** IMPLEMENTATION GAP / DOCUMENTATION GAP\
**Evidence:** `docs/emergency-node.md` (“showduino.com follow-up if needed”), `protocol/showduino_shdo.h` (`estop-node-pixels`), `web/studio-v4-overlay/js/showduino-pixel-authoring.js` (Audio pixel route present; ESTOP pixel route not equivalently surfaced in the overlay inventory helpers reviewed)

Firmware, P4 `EmergencyNodeLink` and SHDO compilation support `ESTOP:NODE:PIXEL:` / `estop-node-pixels`. Studio authoring inventory for ESTOP pixel outputs is not yet as complete as Audio Node pixel routing.

### Required resolution

Surface ESTOP pixel outputs consistently in Studio inventory/authoring, then update the Operator Guide lighting section if the operator-visible labels change.

---

## Gap closure rule

A gap is not closed merely because code has been written. Safety, radio, power, update and hardware-output gaps require the appropriate physical acceptance evidence. When a gap closes, update:

1. the relevant firmware/documentation;
2. `SHOWDUINO_MANUAL_SOURCE_MATRIX.md`;
3. the affected customer-manual section;
4. the manual revision/baseline SHA.

## GAP-022 — P4 emergency ambience override and playback acceptance

**Status:** RESOLVED (emergency/STOP software in this update) — HARDWARE VALIDATION REQUIRED

The reviewed baseline had no ambience call in normal STOP and only silenced PCM5102A during Emergency. This update stops normal ambience on `SHOW:STOP` / `STOP:ALL`, replaces it with the canonical emergency WAV on assertion, and stops the announcement on clear without restoring ambience. Saved ambience volume cannot mute the emergency loop; ordinary audio commands cannot replace/stop it while latched. Missing/invalid WAV or I2S failure leaves normal audio stopped and Emergency blocked. Lamp interrupts its normal prop audio and loops its local emergency WAV while latched; P4 ES8311 retains its dedicated emergency-sound role.

The parser accepts mono but writes samples directly to a stereo I2S slot configuration without mono expansion. The nonblocking I2S write does not retry unwritten bytes. Physical continuity, mono correctness and simultaneous ES8311/PCM5102A playback remain unproven; use stereo PCM for the initial bench test. No persistent AMBIENCE cue type or dedicated Director/Studio workflow exists.

## GAP-023 — Generic I/O frontends and scene binding

**Status:** PARTIALLY RESOLVED (local engine) — UI/INTEGRATION GAP / HARDWARE VALIDATION REQUIRED

`ShowduinoIO.cpp` provides GPIO46/47 mode/polarity/pull/debounce configuration, SD persistence, input events and output pulses. Emergency and normal stop force inactive outputs without restore. P4 USB/service commands exist; the browser allowlists have not added `IO:*`, and no dedicated Director/Studio editor, persistent IO cue type or automatic input-to-scene binding exists. Commission external interfacing, inactive polarity, SD reload, pulse expiry and Emergency/STOP before treating these lines as accepted installation features.
