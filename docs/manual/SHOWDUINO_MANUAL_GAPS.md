# Showduino Commercial Manual — Verification Gaps

**Manual baseline date:** 16 September 2026  
**Repository:** `sumkindafreak/new_showduino`  
**Baseline branch:** `main`  
**Baseline commit:** `9f304cdc65786c0e8d306987010a97eb5a3ad587`  
**Product baseline:** Showduino `1.0.0-rc.1`

This document records anything that could not honestly be presented as finished commercial product behaviour during the repository-verified manual pass. It is deliberately conservative: uncertainty belongs here, not disguised as customer-facing fact.

The categories used are:

- **IMPLEMENTATION GAP** — approved or documented behaviour is not implemented at this baseline.
- **DOCUMENTATION GAP** — code appears to define behaviour but the product documentation is incomplete or contradictory.
- **UI/DOCUMENTATION MISMATCH** — operator wording or workflow differs between code/UI and documentation.
- **HARDWARE VALIDATION REQUIRED** — source exists but physical acceptance is not complete.
- **SAFETY PROCEDURE TO CONFIRM** — safety-related behaviour or operator procedure needs explicit acceptance.
- **VERSION INFORMATION REQUIRED** — component/release version sources do not agree.
- **PRODUCT DECISION REQUIRED** — behaviour needs an owner decision before it should be frozen into a commercial manual.

---

## GAP-001 — Main Emergency button 8-second Locate hold

**Category:** IMPLEMENTED — HARDWARE ACCEPTANCE REQUIRED  
**Severity:** Release-blocking until physical bench sign-off  
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

**Category:** IMPLEMENTED — HARDWARE ACCEPTANCE REQUIRED  
**Current code:** `DirectorLocateScreen.*`, `DirectorAmbientPixels.cpp/.h`, `backlight.cpp`, `touch_lvgl.cpp`

Implemented in firmware:

- `DIRECTOR:LOCATE` wakes the display and forces backlight on;
- normal auto-off is temporarily suspended without rewriting saved settings;
- ambient red/blue locator flashes with **no 15-second timeout**;
- **LOCATE ACTIVE** overlay states that Emergency remains active;
- the first deliberate touchscreen press acknowledges Locate only;
- that press/release cycle is consumed and cannot click an underlying control;
- Locate acknowledgement does not clear Emergency.

Hardware-test sleep/wake, touch consumption and emergency-latch retention before treating this as commercially validated.

---

## GAP-003 — Emergency clear is Director request → confirm (physical button never clears)

**Category:** IMPLEMENTED — HARDWARE ACCEPTANCE REQUIRED  
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

**Category:** SAFETY PROCEDURE TO CONFIRM / PRODUCT DECISION REQUIRED  
**Current code:** P4 `ShowduinoStageEngineP4.ino`, `EmergencyInput.cpp`

The authoritative P4 variable `emergencyLocked` is initialised `false` at boot. No persistent restore of that latch was found in the active P4 code during this pass.

A physically asserted button at boot should be detected again by the input service after debounce, but that is not the same thing as persisting a previously latched momentary-button emergency through complete power removal after the button has been released.

### Required resolution

Decide the required commercial behaviour for reboot/power loss during a latched Emergency and physically validate it. Until then the manual must not state that the latch survives full power removal.

---

## GAP-005 — Emergency/clear physical acceptance remains incomplete

**Category:** HARDWARE VALIDATION REQUIRED / SAFETY PROCEDURE TO CONFIRM  
**Primary evidence:** `docs/physical-test-checklist.md`, `docs/v1-feature-matrix.md`

The repository explicitly states that source inspection is not physical acceptance. Release-blocking checks include:

- physical GPIO25 latch;
- output emergency states;
- no automatic show resume;
- Director clear workflow;
- GPIO23/GPIO24 emergency-white behaviour;
- node emergency behaviour.

The dated physical-acceptance record states that no attached Showduino hardware was available on the validation host and the physical items remain not run/blocked.

### Required resolution

Complete and sign off the physical emergency matrix before `v1.0.0` and before a final production manual revision.

---

## GAP-006 — Full first-owner authoring journey is not yet one finished product workflow

**Category:** IMPLEMENTATION GAP / UI/DOCUMENTATION MISMATCH  
**Primary evidence:** `docs/studio/README.md`, `web/showduino-studio/README.md`

The intended commercial journey is conceptually:

`Create attraction/show → create scene → add devices → build timeline → validate → deploy → play`.

At this baseline, two different surfaces share the Studio name:

1. the S3-hosted **system console / commissioning WebUI**, which is active;
2. the broader **Studio V4 / SHDO v2 authoring** model, which is not fully integrated into the on-device workflow.

The live S3 system console is explicitly *not* a complete on-device SHDO authoring Studio. The full attraction/scene terminology is not present as the active current system-console workflow.

### Required resolution

Complete and freeze the customer-facing authoring/deployment workflow, then capture its exact current labels/screens in the manual. Until then, the main manual distinguishes live commissioning/deployment capability from the target authoring experience.

---

## GAP-007 — Persistent production format cannot yet represent the full theatrical feature set

**Category:** IMPLEMENTATION GAP  
**Primary evidence:** `docs/production-storage.md`, `docs/audio-pixel-engine.md`

P4 persistent production format v1 accepts only bounded `TEST`/`LOG` cues. Local P4 pixel control, Audio Node playback and RAM timeline commissioning exist, but persistent production-file `PIXEL` and `AUDIO` cue parsing/routing are not complete production-format-v1 behaviour.

### Required resolution

Complete the persistent production cue model/compiler/runtime path for the intended customer device types, with validation and migration rules, before the commercial manual describes full mixed-device shows as a finished persistent workflow.

---

## GAP-008 — “Studio” naming currently covers two materially different surfaces

**Category:** PRODUCT DECISION REQUIRED / DOCUMENTATION GAP  
**Primary evidence:** `docs/studio/README.md`, `web/showduino-studio/README.md`

“Studio” currently refers both to the S3-hosted system/commissioning console and to the richer SHDO v2 authoring experience/blueprint. This can confuse owners reading a commercial manual.

### Required resolution

Freeze customer-facing names for:

- the system/commissioning WebUI;
- the production authoring/timeline application;
- the `/studio/` embedded authoring snapshot.

Then use those names consistently in firmware, browser navigation and documentation.

---

## GAP-009 — Default SoftAP credential is a bench credential, not a finished venue provisioning story

**Category:** PRODUCT DECISION REQUIRED / HARDWARE VALIDATION REQUIRED  
**Current source:** Comms `BoardConfig.h`

The current Communications S3 SoftAP is:

- SSID: `Showduino`
- WPA2 password: `showduino`

The code itself labels this as a documented **bench** secret and says it must be changed before a public venue. During this pass, an operator-facing workflow for changing the Showduino SoftAP password itself was not verified; the Network page configures optional home/venue STA credentials.

### Required resolution

Define production provisioning for the Showduino AP credential (unique-at-build, first-run change, managed setting, or another explicit policy) before public deployment guidance is finalised.

---

## GAP-010 — Network configuration is implemented but still awaiting bench acceptance

**Category:** HARDWARE VALIDATION REQUIRED  
**Primary evidence:** `docs/v1-feature-matrix.md`, `docs/physical-test-checklist.md`

AP+STA behaviour, retained SoftAP, radio-channel following, Director/Audio/Lamp reconnect and non-channel-1 venue Wi-Fi operation are implemented foundations but remain on the V1 hardware acceptance list.

### Required resolution

Run the release-blocking radio/power-cycle matrix, especially Director stability while specialist nodes join and while venue Wi-Fi changes the operating channel.

---

## GAP-011 — Comms self-OTA exists, but system-wide OTA does not and physical OTA proof remains a gate

**Category:** HARDWARE VALIDATION REQUIRED / DOCUMENTATION GAP  
**Primary evidence:** `docs/comms-ota.md`, `docs/system-updates.md`, Comms OTA source

Current code supports **Communications Controller self-OTA only**, with HTTPS download, SHA-256 verification, inactive-slot write, health gate and software rollback. P4, Director and specialist nodes remain USB-update components at this phase.

Some older documents still say there is no OTA install at all. Those statements are stale relative to current HEAD.

### Required resolution

Physically prove normal Comms OTA and failed-health rollback on the current hardware/firmware combination. Update older architecture/network prose when the OTA milestone is accepted.

---

## GAP-012 — Release manifest component versions lag current `main`

**Category:** VERSION INFORMATION REQUIRED  
**Sources:** `releases/showduino-1.0.0-rc.1.manifest.json`, current component `BoardConfig.h`

The committed RC release manifest lists P4 `0.6.3` and Comms `0.5.1`, while current HEAD source identifies P4 `0.6.4` and Comms `0.5.2`. Director is listed as `0.9.7-director` in the manifest, but no equally clear Director firmware-version constant was identified in the active Director BoardConfig during this pass.

### Required resolution

Publish/update an authoritative release inventory that matches the exact binaries accepted for a manual revision. The user manual therefore uses product version `1.0.0-rc.1` plus the exact repository SHA as its strongest baseline identifier.

---

## GAP-013 — Director OS 2.0 source exists but is not the active baseline UI

**Category:** UI/DOCUMENTATION MISMATCH  
**Current source:** Director `BoardConfig.h`

`SHOWDUINO_OS2_SHELL` is `0` at the baseline SHA. The active Director is therefore the current `ShowduinoUi` page set, even though newer OS 2.0 app/service source exists in the tree.

### Required resolution

When the OS 2.0 shell becomes the shipping UI, revise the Director chapter and screenshots as one versioned manual change. Do not document disabled source as current behaviour.

---

## GAP-014 — Current Director page set and diagnostics are still changing

**Category:** DOCUMENTATION GAP / HARDWARE VALIDATION REQUIRED  
**Sources:** Director `DisplayPages.h`, `ShowduinoUi.h`, recent Git history

The active page IDs/titles are verifiable in source, but recent repository history and product work indicate ongoing Director diagnostics/UI refinement. A commercial screenshot set would become stale quickly.

### Required resolution

Freeze a release-candidate UI build, capture screenshots from that exact binary and add figure references to a later manual revision.

---

## GAP-015 — P4 local pixel hardware is implemented but not signed off

**Category:** HARDWARE VALIDATION REQUIRED  
**Sources:** `docs/hardware-pinout.md`, `docs/audio-pixel-engine.md`, `docs/physical-test-checklist.md`

GPIO23 segmented Show Pixels and GPIO24 emergency/designated-signage pixels are implemented in source. The repository still marks the physical pixel paths as requiring hardware commissioning.

### Required resolution

Validate configured lengths, logic buffering, current capacity, all-white emergency load, segment behaviour, clear-to-blackout and signage grouping on production-equivalent wiring.

---

## GAP-016 — Specialist node maturity is mixed

**Category:** HARDWARE VALIDATION REQUIRED  
**Sources:** `docs/repository-status.md`, node-specific documentation

At this baseline:

- Audio Node — implemented, hardware acceptance still required;
- S3 Lamp Node — active, physical pins confirmed, some sensor details remain unconfirmed;
- C3 Pixel Node — active, hardware test required;
- C3 Emergency Node — software implemented, GPIO unconfirmed, not physically validated;
- MOSFET Node — planned/not implemented;
- Relay Node — legacy/retired.

### Required resolution

Only ship/manualise a node as a normal supported accessory once its hardware acceptance is complete. The main manual labels current RC maturity and does not present MOSFET/legacy Relay as shipping features.

---

## GAP-017 — Wireless Emergency Nodes are not equivalent to the main momentary Emergency button

**Category:** DOCUMENTATION GAP / SAFETY PROCEDURE TO CONFIRM  
**Source:** `docs/emergency-node.md`

The separate wireless Emergency Node design uses a local normally-closed loop, local latch and **assert-only** wireless policy. Its physical control and re-arm model differ from the main Showduino momentary GPIO25 button.

### Required resolution

Keep accessory-specific instructions separate. Never copy the main-button Locate/clear gesture into the Emergency Node chapter unless its own firmware explicitly implements it.

---

## GAP-018 — Commercial enclosure connector labelling cannot be verified from firmware alone

**Category:** HARDWARE VALIDATION REQUIRED / PRODUCT DECISION REQUIRED

The repository defines board-level pins and electrical expectations, but this pass did not identify a frozen commercial enclosure drawing, rear-panel connector schedule, fuse specification, PSU rating or production wiring diagram that maps those internal resources to final customer-accessible labels.

### Required resolution

When enclosure/loom/power design is frozen, add controlled hardware drawings and rated connector/power specifications. Until then, the manual's Hardware Tour remains functional rather than inventing panel labels.

---

## GAP-019 — No certification claims are supported by the repository

**Category:** PRODUCT DECISION REQUIRED

No evidence was found that Showduino is certified as a fire alarm, evacuation system, machinery E-stop, safety PLC, SIL-rated system or PL-rated system.

### Manual consequence

The commercial manual explicitly keeps venue/fire/machinery/life-safety compliance separate from Showduino show-control emergency behaviour. This is not a negative product claim; it is an accuracy boundary until formal certification exists.

---

## GAP-020 — Director touchscreen calibration is implemented but not hardware-accepted

**Category:** IMPLEMENTED — HARDWARE ACCEPTANCE REQUIRED  
**Current code:** `TouchCalibrationMath.h`, `TouchCalibrationStore.cpp`, `DirectorTouchCalibrationScreen.*`, `touch_lvgl.cpp`

Five-point affine calibration, NVS persistence (`showduino_touch` / `cal`), factory fallback, Settings entry, USB `TOUCH:STATUS` / `TOUCH:RESET` / `TOUCH:CALIBRATE`, Emergency/Locate abort, and first-touch consumption while the wizard is open are in firmware.

Do not treat corner accuracy, reboot persistence, or overlay consumption as commercially validated until the physical checklist is signed.

---

## Gap closure rule

A gap is not closed merely because code has been written. Safety, radio, power, update and hardware-output gaps require the appropriate physical acceptance evidence. When a gap closes, update:

1. the relevant firmware/documentation;
2. `SHOWDUINO_MANUAL_SOURCE_MATRIX.md`;
3. the affected customer-manual section;
4. the manual revision/baseline SHA.
