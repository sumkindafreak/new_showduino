# Showduino Director — Operator Guide

**19 November 2026 · 1.0-RC rewrite** · [Firmware baseline `41c7c71`](https://github.com/sumkindafreak/new_showduino/tree/41c7c71ac617abdf9feeedf2064e6b5a9f31cc15). This describes the **current repository implementation**, not proof of the firmware on any particular flashed Director.

## What the Director does

The Director is the **JC8048W550C 800 × 480 ESP32-S3 touchscreen** with **GT911** touch. It is the operator's desk, **not** the show computer: commands go over ESP-NOW to the Comms S3 and UART to the P4. Do not assume a command completed until the P4 confirms its resulting state.

The active firmware uses LVGL pages. The optional OS2 shell source is **disabled** in the reviewed `BoardConfig.h` (`SHOWDUINO_OS2_SHELL 0`).

## Start-of-shift routine

1. Confirm the enclosure is safe and the site's independent safety systems are operational.
2. Boot P4, Comms S3 and Director, then required nodes. Check link and P4 readiness on the Director.
3. Check loaded production, cue count/readiness, faults and node status. Open **NODES** for individual device information.
4. Verify Emergency response and clear with **safe test outputs only**, following the site's approved procedure.
5. Run a controlled rehearsal and record the result. Never leave a potentially dangerous prop energised while diagnosing a connection.

## Active screens and what they mean

| Page | Operator purpose | Important boundary |
| --- | --- | --- |
| **HOME** | Show overview; go to productions, controls, Cue Library, Outputs | A tile is not a direct P4 output |
| **PRODUCTIONS** | Select and request loading of SD productions | Load confirmation comes from P4 |
| **SHOW DETAILS** | Review the selected show | Details may be unavailable until P4 reports |
| **LIVE** | Show state, controls and cue progress | Don't mistake stale status for a live timeline |
| **CUE LIBRARY** | Native **playback cue monitor**, Follow/Browse/scene checkpoint | **Not** the Website Studio editor |
| **NODES** | Discover and inspect specialist node status | Offline means unavailable; no invented states |
| **AUDIO NODE** | Status and applicable audio-node controls | Attraction playback is on the Audio Node |
| **LAMP NODE** | View/control supported managed lamp features | Installed lamp firmware may differ |
| **OUTPUTS** | MOSFET, P4 I/O and I²C-related commissioning | P4 determines output permissions |
| **DIAGNOSTICS** | Faults, communications, recovery evidence | Diagnosis does not waive safety gates |
| **SETTINGS** | Display settings and calibration | Settings changes must not affect Emergency authority |
| **AUDIO** | P4 Showduino **system** sound/volume | Not programme music routing |
| **SYSTEM LOGS** | See reported events | Capture logs for unresolved failures |

The reviewed `DisplayPages.h` also defines context-dependent full-screen pages for Emergency, connection loss, missing SD, reboot, update, recovery, discovery and completion. A listed page does **not** prove every feature has physical acceptance.

## LIVE playback-status refresh — 20 November source update

On the Director **LIVE** page, the **Live Status** action now requests a fresh `SHOW:STATE?` report through Comms from the P4. It is a **status query, not a show-control or restart command**. The interface distinguishes a pending response, a received report, a rejected send and a timeout. The reviewed refresh state machine uses a **3-second response timeout** and shows terminal feedback for approximately **2.5 seconds**. A later send-failure notification must not overwrite a playback report already received. These are source-level behaviours introduced on 20 November (`bb2752a`, `04b67fb`, `af08fe7`); host regression coverage exists, but they are **not yet confirmed on the flashed bench Director**.

If the request fails or times out, do not treat the last displayed playback state as newly verified. Inspect P4/Comms connectivity and logs. Do not infer that a running P4 show stopped just because the Director cannot refresh it. This improvement is useful for observing LOAD → START → COMPLETE acceptance, but does **not** replace P4-side completion evidence or physical safety tests.

## Cue Library — important correction

Older manuals said the HOME **CUE LIBRARY** tile was disabled. This is **outdated for the reviewed GitHub source**. `page_01_home.cpp` enables it, and `DirectorCueLibrary.cpp` implements a native cue/timeline monitor with **FOLLOW**, **PREVIOUS**, **NEXT**, **ALL CUES**, **START SCENE** and **CLOSE**.

**FOLLOW** tracks the current/next cue; browsing lets the operator inspect other rows. **START SCENE** is a deliberate, confirmed playback request and must not be used as casual navigation. The interface displays stale playback information as stale and closes on Emergency. This screen does **not** create, drag or export SHDO clips. See the [Studio Cue Library Guide](SHOWDUINO_STUDIO_CUE_LIBRARY_GUIDE.md).

**Bench note:** The owner previously reported a Cue Library navigation/display fault and intermittent Director freezes. The reviewed code is not proof those symptoms have disappeared on the installed Director. Record a hardware retest before release.

## Production controls

- **Load:** choose a saved production; wait for P4 loaded status.
- **Run/Start:** only when P4 linked and ready. The Director may disable RUN when link/readiness is missing; do not attempt to override it from another surface.
- **Pause/Resume/Stop:** requests require authoritative P4 feedback. A lost Director link does not itself stop an already running P4 show.
- **Finished:** record the completed result. Do not assume automatic restart.

Don't make editing, test, or troubleshooting changes during a running show unless specifically authorised by the installed system and the venue procedure.

## Outputs and node control

The MOSFET output desk shows an **Actual** value reported by the node and a separate requested **Set** value. Moving a level slider alone must not energise an output; **APPLY** requests it. **OFF** and **ALL OFF** are safe-off controls. Level changes are locked while running/paused, in Emergency, during faults or on link loss; safe-off controls have their own guarded path.

Never interpret a sent command as measured load power. Confirm via node feedback **and** physical observation. For wiring and testing, follow [Director Outputs](../director-outputs-mosfet.md) and [Hardware Pinout](../hardware-pinout.md).

## Emergency clear — deliberate and separate

1. Emergency is **latched at the P4** when asserted. Protect people first and remove the cause. The physical button must be **released** before any clear is accepted.
2. From the **Director Emergency screen**, select **CLEAR EMERGENCY**.
3. The P4 may reject the request; otherwise the Director presents **Clear Emergency Stop?** with **CANCEL** and **CONFIRM CLEAR**.
4. Press **CONFIRM CLEAR** only after checking it is safe. Wait for the **P4-authoritative clear result**; if it remains pending, rejects or times out, stop and investigate.
5. **No automatic show resume.** The operator must make an independent start/resume decision.

A continuous **8-second hold** on the physical Emergency button invokes **Locate** in addition to keeping Emergency asserted. The first Director acknowledgement touch is consumed by Locate; it does **not** press an underlying control or clear the latch. This is **not** a 3-second or 8-second clear gesture.

During faults, do not use a reboot to claim an authorised clear. Never bypass separate venue fire, evacuation, electrical or machinery safety systems.

## Display, sleep and calibration

The Director can dim/turn off the **backlight** after a configured timeout; the MCU continues running. Touch wakes it. Emergency/Locate override normal backlight behaviour. The **Settings → Display → CALIBRATE** operation uses the source-defined five-point touch wizard and saved calibration; abandon/retest if touch or Emergency is unstable. Hardware acceptance of calibration remains separate.

On the JC8048W550C the **GT911 is the active touch controller**; do not probe conflicting XPT2046 CS wiring or enable USB CDC on boot over the GT911 SDA/SCL GPIO19/20. Technicians should use [Director BoardConfig](../../firmware/director-esp32-8048s050/ShowduinoDirector8048S050/BoardConfig.h).

## Troubleshooting in plain English

| Symptom | Operator response |
| --- | --- |
| Director says **CONNECTION LOST** | Check power, Comms and P4 link. Do not assume show stopped; preserve state. |
| **P4 OFFLINE** in browser | The S3 may still host the page. Disable operational commands until P4 responds. |
| **NO SD CARD** / missing show | Stop deployment. Check safe power/SD state; do not hot-swap into unsafe operation. |
| Emergency won't clear / **AWAITING STAGE** | Check physical release, P4 state and logs. Stop; **never reset as a clearance workaround**. |
| Cue Library doesn't close or display is stale | Record the firmware version and navigation/serial evidence, stop unsafe operation. |
| MOSFET Actual differs from Set | Trust feedback and physical test, not the last requested level. |
| Touch inaccurate | Use calibration only when stable; preserve the current calibration before altering it. |
| Audio missing | Check Audio Node SD, state, volume and routing; P4 system audio is separate. |

## References

[Quick Start](SHOWDUINO_QUICK_START.md) · [User Manual](SHOWDUINO_USER_MANUAL.md) · [Pre-Opening Checklist](SHOWDUINO_PRE_OPENING_CHECKLIST.md) · [Source Matrix](SHOWDUINO_MANUAL_SOURCE_MATRIX.md) · [Verification Gaps](SHOWDUINO_MANUAL_GAPS.md).
