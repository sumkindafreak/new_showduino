# SHOWDUINO

## Commercial Show Control System

# User Manual

**Manual revision:** 0.1-RC — repository-verified documentation baseline  
**Applicable product:** Showduino `1.0.0-rc.1`  
**Repository baseline:** `main` at `9f304cdc65786c0e8d306987010a97eb5a3ad587`  
**Baseline date:** 16 September 2026  
**Status:** Release-candidate manual. Hardware acceptance and identified product gaps remain open.

---

## Read this first

Showduino is still at a release-candidate stage. This manual describes the behaviour that could be verified from the repository baseline above. It deliberately does **not** present planned features as finished product functions.

Where current firmware is implemented but has not yet passed the repository's physical acceptance gate, this manual says so. Where a newly approved product requirement differs from the firmware at this baseline, the discrepancy is recorded in `SHOWDUINO_MANUAL_GAPS.md` rather than being disguised as current behaviour.

For day-to-day use, operators should also have access to:

- `SHOWDUINO_QUICK_START.md`
- `SHOWDUINO_PRE_OPENING_CHECKLIST.md`

---

# 1. About Showduino

Showduino is a modular show-control system for attractions, escape rooms, immersive theatre, exhibitions, scenic effects and other installations where lights, audio and specialist effect devices need to operate in a timed and repeatable way.

The system is designed around a simple responsibility model:

> **The Show Engine decides. The Communications Engine transports. The Director commands and displays. The Nodes act.**

In normal use, an operator should not need to understand the embedded electronics behind that model.

## 1.1 The main parts

### Show Engine

The **Show Engine** is the authoritative controller. It owns the live show state, timeline, Emergency state, production loading and the final decision to accept or reject an operator request.

The current Show Engine uses the ESP32-P4 platform, but the important customer-facing fact is its role: **if another screen or device disagrees with the Show Engine, the Show Engine is authoritative.**

### Communications Controller

The **Communications Controller** carries messages between the Director, browser, specialist Nodes and Show Engine. It also hosts the local browser interface and can connect to optional home/venue Wi-Fi.

It does not make show decisions on behalf of the Show Engine.

### Director

The **Director** is the portable 800 × 480 touchscreen operator interface. It is used to view system state and request actions such as loading, starting, stopping and monitoring a production.

A Director button press is a request. The interface should reflect the state confirmed by the Show Engine rather than pretending an action succeeded merely because the screen was touched.

### Specialist Nodes

Specialist Nodes perform functions away from the main controller. Current repository work includes Audio, Lamp, Pixel and Emergency Nodes at different levels of physical validation.

Nodes act on requests routed by the Show Engine. They are not independent show authorities while Showduino owns them.

### Studio / browser interface

The Communications Controller hosts a local browser interface for system status, commissioning, productions, outputs, devices, networking and diagnostics.

The repository currently uses the name **Studio** for more than one related surface. The on-device browser console is active, while the complete long-term attraction/scene authoring workflow is still being integrated. Section 10 explains this boundary.

## 1.2 Local operation is deliberate

A loaded/running show is intended to execute locally on the Show Engine. It must not depend on:

- an open browser window;
- the Director remaining available;
- a home or venue Wi-Fi connection;
- an Internet connection;
- a cloud service.

Loss of Wi-Fi or Internet access must therefore not be confused with loss of the Show Engine itself.

---

# 2. Safety information

Show control sits beside real people, scenery, electrical loads, loud audio and moving or illuminated effects. Operational decisions must therefore follow this order:

1. **People first**
2. **The show second**
3. **Hardware third**

If an incident involves a person, do not delay a real-world response to protect a scene, complete a cue, preserve a prop position or diagnose the controller.

## 2.1 Callout meanings

**DANGER** — reserved for a direct hazard where failure to follow the instruction could result in death or serious injury. Use only where the installation documentation identifies such a hazard.

**WARNING** — a condition that could create a serious safety risk if handled incorrectly.

**CAUTION** — a condition that can damage equipment, wiring, media or an attraction installation.

**IMPORTANT** — information that is essential for correct Showduino operation or recovery.

**NOTE** — useful supporting information.

## 2.2 Showduino and venue safety systems

**WARNING — Showduino show-control Emergency behaviour must not be represented as a substitute for any certified safety system required by the installation.**

The repository does not establish Showduino as a certified fire alarm, evacuation system, machinery E-stop, safety PLC, SIL-rated system or PL-rated system.

Venue fire safety, emergency lighting, evacuation, machinery guarding, electrical protection and other legally or operationally required safety systems remain separate responsibilities.

## 2.3 Electrical installation

Electrical installation and external loads should be designed, protected and inspected by a competent person appropriate to the installation.

Do not assume a Showduino logic output can directly power a lamp, motor, solenoid, high-current LED installation or mains load. Use the correct driver, interface, power supply, fuse/protection and cable rating for the actual load.

The P4 Plug-in Bus is a **3.3 V I²C bus**. SDA/SCL must not be pulled to 5 V. A 5 V I²C module requires an appropriate bidirectional level shifter.

Addressable-pixel installations should use a correctly sized external supply. The repository standard for final P4 pixel data wiring is a 5 V-compatible logic buffer where required, a 470 Ω series resistor near the controller-side driver, common controller/pixel grounds and suitable bulk capacitance/current distribution.

## 2.4 Inspection before operation

Before opening an attraction:

- inspect the Showduino unit, Director and Nodes for damage;
- confirm power and external-load wiring is secure;
- verify the Emergency control operates correctly;
- confirm required Nodes are online;
- check the intended production is loaded;
- test outputs in a controlled state before admitting guests;
- resolve unexpected Emergency, fault or offline indications rather than bypassing them.

Use the dedicated pre-opening checklist supplied with this manual.

---

# 3. System overview

```text
                         SHOWDUINO SYSTEM

                     Director Touchscreen
                              |
                           ESP-NOW
                              |
                  Communications Controller
                    /          |           \
       local browser/WebUI     |            specialist Nodes
                               |             Audio / Lamp /
                          UART 115200        Pixel / Emergency
                               |
                               v
                         SHOW ENGINE
                         (authority)
                               |
                +--------------+--------------+
                |              |              |
            system audio   show pixels   signage/emergency
             (local)        (GPIO23)       pixels (GPIO24)
```

The browser path is:

```text
Phone / tablet / computer
        |
      Wi-Fi
        |
Communications Controller
        |
   API proxy / UART
        |
    Show Engine
```

The browser does not become the show clock. Closing the browser does not stop an already running P4 show.

---

# 4. Hardware tour

This revision describes functions rather than final enclosure-panel labels. A frozen commercial enclosure connector schedule has not yet been verified in the repository.

## 4.1 Main Showduino assembly

The current architecture contains two main processing roles:

- **Show Engine (P4)** — show authority, local storage/runtime, system audio, local pixel engines and hardwired Emergency input;
- **Communications S3** — Director/Node radio transport, P4 UART link, local Wi-Fi access point and browser host.

These are separate roles even when they are installed inside one enclosure.

## 4.2 Director touchscreen

Current Director hardware is an ESP32-S3-based 800 × 480 capacitive touchscreen. Its operator-facing functions include productions, live show status, Nodes, diagnostics, settings, system logs and Emergency presentation.

Two addressable ambient LEDs provide state/attention indications. They are presentation indicators, not show-output pixels.

## 4.3 Main Emergency button

The main Showduino Emergency input is a **momentary pushbutton**, wired to the P4's local Emergency input. The software latch remains active when the button is released.

This must not be confused with separate wireless Emergency Node accessories, which use their own local normally-closed input/latch design.

## 4.4 P4 local show-pixel line

The current local theatrical pixel output is the P4 **Show Pixel Line**. It supports segmented effects in firmware.

It is separate from the dedicated Emergency/signage pixel output.

## 4.5 Emergency/designated-signage pixel line

The dedicated signage line is safety-owned by Showduino, not by the production timeline. The current model groups pixels into ten-pixel sign groups:

- normal: first pixel in each group green, remaining nine off;
- Emergency: all pixels in all groups bright white.

## 4.6 P4 system speaker/audio

The P4 onboard ES8311 audio path is reserved for Showduino system/safety sounds such as boot, Emergency and operator notification sounds.

Attraction/programme audio is handled by a specialist **Audio Node**. Showduino does not silently fall back to the P4 system speaker for attraction audio.

## 4.7 Storage

The Show Engine uses an SD card for configuration, persistent productions and system assets. The SD card is the persistent storage backbone, but it is **not** the safety backbone: Emergency handling must not depend on an SD file being available.

## 4.8 USB/service connections

USB remains a service/recovery path for components that do not yet support operator OTA updates. It is not intended as the normal day-to-day operator interface.

---

# 5. Before first power-on

For an assembled commercial unit, follow the installation information supplied with that specific hardware revision. For the current RC architecture, verify the following before applying power.

## 5.1 Physical condition

Check that:

- the enclosure and display are undamaged;
- no exposed conductor can short against the enclosure;
- data and power connectors are fully seated;
- external output interfaces are suitable for their loads;
- the Emergency button moves freely;
- SD cards required by the configured system are fitted;
- speakers and audio wiring are connected to the correct audio device;
- addressable-pixel supplies are correctly rated and grounded.

## 5.2 Power

Use regulated supplies suitable for the final Showduino assembly and connected accessories. Do not power substantial external pixel loads through the P4 board.

## 5.3 Outputs

For first commissioning, keep potentially hazardous or startling external effects isolated until the control state is known. Confirm status/diagnostics first, then commission outputs individually.

## 5.4 Network

Home/venue Wi-Fi is optional. A first bench setup can use the Showduino local access point without Internet access.

Current RC Communications firmware uses:

- SSID: **Showduino**
- bench password: **showduino**
- default access-point address: **192.168.4.1**

**IMPORTANT:** the firmware source explicitly identifies `showduino` as a bench credential that must be changed before public-venue use. A finished operator provisioning workflow for changing the Showduino AP password is not yet verified at this baseline. See the gaps document.

---

# 6. First power-on

## 6.1 Recommended RC power sequence

The repository physical-test procedure uses:

1. power the **P4 Show Engine**;
2. power the **Communications Controller**;
3. power the **Director**;
4. allow configured specialist Nodes to start and join.

A future integrated enclosure may automate this sequence; follow the hardware revision supplied with the unit.

## 6.2 What to expect

On a healthy start:

- the P4 initialises its core services and reaches its ready state;
- the Communications Controller establishes its P4 UART link and local `Showduino` Wi-Fi;
- the Director searches for and establishes its Communications link;
- the Director requests authoritative state from the P4;
- configured Nodes announce and obtain Show Engine ownership where appropriate.

Do not rely on a fixed boot-time promise. Hardware and storage discovery can vary.

## 6.3 Ready versus connected

Treat these as separate questions:

- **Is the Director linked to Communications?**
- **Is the Communications Controller linked to the P4?**
- **Is the P4 ready and authoritative?**
- **Are the Nodes required by this production online?**
- **Is optional home/venue Wi-Fi connected?**
- **Is the Internet available?**

A Wi-Fi or Internet problem does not automatically mean the Show Engine has failed.

## 6.4 First browser connection

For the current RC bench setup:

1. connect a phone, tablet or computer to Wi-Fi **Showduino**;
2. use the current bench password **showduino**;
3. open `http://192.168.4.1/` for the local system console;
4. the optional embedded authoring surface is available at `/studio/` on the same address.

No Internet connection is required for the local interface.

---

# 7. Director touchscreen

The current baseline uses the active `ShowduinoUi` page set. A newer OS 2.0 shell exists in source but is disabled at this baseline and is therefore not documented as the shipping UI here.

## 7.1 HOME

**What it does**  
Provides the main system overview and current availability/status information.

**When to use it**  
At startup, before opening and whenever an operator needs a quick health check.

**Look for**

- Show Engine/Communications link state;
- production/show state;
- Node availability;
- Emergency or fault indications.

## 7.2 PRODUCTIONS

**What it does**  
Shows productions visible to the current Director/storage model and allows an operator to select/load a production.

**When to use it**  
Before running an attraction or changing to a different installed production.

**Important**  
Loading a production and starting it are separate actions. Deploying a production also does not auto-start it.

## 7.3 SHOW DETAILS

**What it does**  
Shows information about the selected/loaded production.

**When to use it**  
To confirm the intended production before operation.

## 7.4 LIVE

**What it does**  
Displays the authoritative running-state mirror, including timing/cue progress where available, and presents show-control requests.

**When to use it**  
During normal attraction operation.

**Important**  
A control request is not considered successful merely because it was touched. Watch the confirmed P4 state.

## 7.5 NODES

**What it does**  
Displays specialist-node availability/status, including current Audio, Lamp, Pixel and Emergency-node foundations where supported.

**When to use it**  
During pre-opening checks and fault diagnosis.

**Important**  
A Node can be offline while the P4 itself remains healthy. Diagnose the failed part rather than power-cycling the whole system automatically.

## 7.6 AUDIO NODE

**What it does**  
Displays Audio Node state and the current control/diagnostic surface.

**When to use it**  
For programme-audio commissioning and status checks.

The P4 system speaker is not a replacement for an offline attraction Audio Node.

## 7.7 DIAGNOSTICS

**What it does**  
Provides system/connection information intended to identify which component is actually unavailable.

**When to use it**  
When the Director, P4, Communications link, storage or Nodes do not match the expected state.

## 7.8 SETTINGS

**What it does**  
Contains current Director/operator settings, including display behaviour and software/network status surfaces where implemented.

**When to use it**  
For routine configuration rather than live show control.

## 7.9 AUDIO

This page relates to the Director/P4 system-audio presentation supported by the current UI. Attraction programme audio remains the responsibility of the Audio Node.

## 7.10 SYSTEM LOGS

**What it does**  
Shows operator/system log information useful for diagnosis.

**When to use it**  
After a fault, rejected request or unexpected connection event.

## 7.11 System screens

The Director also defines full-screen states including:

- **EMERGENCY**
- **CONNECTION LOST**
- **NO NETWORK**
- **NO SD CARD**
- **SYSTEM REBOOT**
- **FIRMWARE UPDATE**
- **BACKUP**
- **RECOVERY**
- **DISCOVERY**
- **SHOW COMPLETE**

Some are fault/state presentations rather than everyday menu pages.

---

# 8. Display sleep / auto-off

The Director does **not** enter MCU deep sleep when the screen turns off. The current implementation dims and turns off the **backlight** while the Director continues operating.

## 8.1 Wake behaviour

Touch/activity returns the backlight to the configured brightness and resets the inactivity timer.

## 8.2 Timeout

The backlight service supports an inactivity timeout in minutes. A timeout value of `0` disables automatic backlight-off.

The implementation dims during the idle period and turns the backlight off at the configured timeout. The saved Director configuration may therefore differ from the service's compiled default.

## 8.3 Emergency and Locate

Emergency presentation is independent of ordinary show operation.

The current baseline's **Locate** implementation is **not considered production-ready for this manual** because it conflicts with the newly approved product behaviour. Current code can wake the display and flash the Director's ambient locator lights, but the production requirement — continuous 8-second hold, forced-on/suspended timeout, `LOCATE ACTIVE`, and first-touch acknowledgement — is not yet fully implemented.

Do not train operators on the current temporary multi-press Locate gesture as a production procedure.

---

# 9. Connecting Showduino to a network

Showduino has two different networking concepts that operators should keep separate.

## 9.1 Showduino local access point

The Communications Controller hosts the local **Showduino** Wi-Fi access point. This is the direct route to the browser interface.

The access point remains available in the intended AP-only and AP+STA modes.

## 9.2 Optional home/venue Wi-Fi

The Communications Controller can also join a home or venue Wi-Fi network. This is optional and is used for functions such as Internet-based update discovery/download.

The current browser **Network** page provides:

- **SSID**
- **Password**
- **Connect**
- **Disconnect**
- **Forget**
- **Scan**
- **AP only**

The page reports the Showduino AP, AP IP, current mode, home Wi-Fi state, saved SSID, STA IP, signal level, radio/ESP-NOW channel and Internet state.

Passwords are not returned in status responses or displayed after storage.

## 9.3 Connecting to venue Wi-Fi

1. Open the browser **Network** page.
2. Select **Scan** if required.
3. Select the venue network or enter its **SSID**.
4. Enter the **Password**.
5. Select **Connect**.
6. Wait for the page to report the connection result.
7. Confirm the Showduino local AP remains available.

The Showduino radio/ESP-NOW channel follows the associated venue access point while connected. Director/compatible Nodes are designed to follow that operating channel while unlinked.

## 9.4 Disconnect and Forget are different

**Disconnect** returns to AP-only operation while keeping the saved venue credentials.

**Forget** removes the saved venue credentials.

## 9.5 If venue Wi-Fi or Internet fails

Do not treat this as an automatic Show Engine failure.

A loaded production is intended to continue locally. Internet is not required to run the show. Update checking/downloads may become unavailable until Internet access returns.

---

# 10. Showduino Studio and the browser interface

This section is deliberately precise because the current repository contains two related concepts under the Studio name.

## 10.1 The active S3-hosted system console

The live local browser console is hosted by the Communications S3 and provides these routes/pages at the current baseline:

- **Home**
- **Productions**
- **Live**
- **Outputs**
- **Devices**
- **Network**
- **System**
- **Settings**

It is the current browser front door for status, commissioning and system management.

## 10.2 The authoring Studio

Showduino also has an SHDO v2 authoring model and Studio V4 work for production creation/timeline editing.

The complete commercial journey of:

```text
CREATE ATTRACTION
        ↓
CREATE SCENE
        ↓
ADD DEVICES
        ↓
BUILD TIMELINE
        ↓
VALIDATE
        ↓
DEPLOY
        ↓
PLAY
```

is the intended product experience, but it is **not yet one fully integrated on-device workflow at this baseline**.

The local browser console must therefore not be described as a complete production authoring system when it currently provides a mixture of commissioning, production inventory and limited deploy/runtime functions.

## 10.3 Editing versus running

The architectural rule remains simple:

- **Studio describes authoring intent.**
- **P4 executes the live show.**

A browser can be closed after a valid production/timeline has been loaded. The live show clock remains on the P4.

---

# 11. Creating an attraction / production

The final customer-facing attraction/scene creation workflow is still an open integration item at this RC baseline.

## 11.1 What is current

The repository has:

- an active SHDO v2 production/interchange contract;
- an S3-hosted browser console;
- a persistent P4 production store;
- an SHDO deployment/compiler path foundation;
- a separate RAM timeline commissioning path for current `PIXEL` and `AUDIO:NODE` commands.

## 11.2 What is not yet safe to promise as a finished workflow

This manual does **not** claim that a new owner can currently complete every step of a polished `New Attraction → Scene → Devices → Timeline → Deploy` flow solely through the live S3 system console.

That gap is explicitly tracked for completion rather than hidden behind developer procedures.

---

# 12. Productions and scenes

## 12.1 Production

A **production** is the installable/run-time show package known to the P4. The persistent current runtime store uses a production folder containing a manifest and timeline.

A production can be loaded without starting it.

## 12.2 Scene terminology

The richer authoring model uses scenes/cues/tracks as part of the intended Studio experience, but the current persistent P4 format is not yet a complete scene-based theatrical authoring runtime.

Where an authoring Studio build shows **Scene**, follow that build's exact UI. Do not manually invent scene files or edit storage JSON unless working under technical/development procedures.

---

# 13. Adding devices

Showduino is intended to let owners work with meaningful devices rather than memorising microcontroller pins.

Current product/device concepts include:

- attraction **Audio Node**;
- specialist **Lamp Node**;
- local **Show Pixel Line**;
- specialist **Pixel Node** foundation;
- **Emergency Node** foundation;
- P4 Plug-in Bus devices;
- future **MOSFET Node**.

The old Relay Node product direction is retired/legacy and must not be used as the basis of a new commercial workflow.

DMX production control is parked/out of scope at this baseline.

## 13.1 Electrical capability still matters

Friendly UI labels do not remove electrical limits. A logical “light” may still require an appropriate external MOSFET, driver, relay/contactor, PSU or certified interface depending on the load.

---

# 14. Timeline editor

The long-term Studio timeline is intended to let an operator place actions against time using tracks/layers and device-oriented controls.

At the current repository baseline, do not confuse that design direction with the P4 persistent production format.

## 14.1 Current RAM timeline commissioning path

The live browser system can send a RAM timeline through `/api/studio-timeline` for current commissioning use. The supported current subset is:

- `PIXEL` commands;
- `AUDIO:NODE` commands.

`autoStart` is false. Deploying the RAM timeline does not automatically run it.

## 14.2 Persistent production timeline

The current persistent P4 production format v1 is intentionally strict and presently accepts only internal `TEST`/`LOG` cue types.

This means a visually complete Studio timeline containing mixed attraction Audio/Pixel/device cues must **not** be described as fully persistable/executable through production format v1 yet.

## 14.3 Timeline safety behaviour

The P4 owns the live timeline clock. If Emergency activates during a running show, the current runtime pauses the timeline and enters the Emergency state. Emergency clear does not automatically resume it.

---

# 15. Productions: load, start, pause, resume and stop

## 15.1 Load

Loading prepares a production on the P4. It does not automatically start the show.

Loading/unloading is rejected while Emergency is active.

## 15.2 Start

Start is accepted only when the P4 considers the show/runtime valid and Emergency is clear.

## 15.3 Pause

Pause stops timeline progression while retaining the loaded production and current position.

## 15.4 Resume

Resume is rejected while Emergency is active.

After an Emergency is legitimately cleared, a previously running show may remain **PAUSED**. Resuming from that point requires a separate deliberate operator action.

**IMPORTANT:** your venue procedure should decide whether resuming from the interruption point is appropriate. For many guest-facing incidents, stopping/reloading/restarting the experience is the safer operational choice.

## 15.5 Stop

Stop terminates active timeline playback. Stop remains a safe request during Emergency; it does not clear the Emergency latch.

---

# 16. Emergency operation

This is the most important operator section in the manual.

## 16.1 If there is a real incident

1. **Deal with people and the physical incident first.**
2. Press the main Showduino **Emergency** button when Showduino-controlled effects need to enter Emergency state.
3. Do not release/clear the system merely because the immediate visual effect has stopped.
4. Follow the attraction's own evacuation, first-aid, fire, machinery or incident procedure as applicable.

## 16.2 What the main button currently does

At this baseline, a valid press of the main momentary button causes the P4 to latch Emergency immediately after input debounce.

Releasing the button does **not** clear Emergency.

The P4 becomes the authoritative Emergency source and commands its supported outputs/Nodes into Emergency state.

## 16.3 Operator-visible Emergency behaviour

Current implemented behaviour includes:

- Director shows the full-screen **EMERGENCY** state;
- running show timeline is paused;
- P4 system Emergency audio may play if the configured asset/audio path is available;
- GPIO23 Show Pixels are overridden to bright white;
- GPIO24 signage pixels are overridden to bright white;
- attraction Audio Node is commanded to stop/mute and does not auto-resume;
- compatible pixel-capable Nodes follow the global white Emergency policy;
- normal show requests that conflict with Emergency are rejected/overridden.

Emergency safety must not depend on the Emergency WAV file successfully playing.

## 16.4 The Director Emergency screen

Current visible text includes **EMERGENCY** and **SHOW STOPPED**, plus source/link/show context where available.

Stray touches on the Emergency overlay are absorbed so controls underneath do not fire accidentally.

## 16.5 Clearing Emergency — current RC baseline

**WARNING:** do not clear Emergency until the real-world reason for the Emergency has been resolved and the attraction is safe to return to a controlled idle/paused state.

At this exact RC baseline, the physical main-button clear request is implemented as a **3-second hold**, followed by a separate Director confirmation. This procedure is expected to change because the product owner has now assigned an 8-second hold to Locate; see the manual gaps document.

For this exact baseline only:

1. confirm the incident is resolved and the attraction is safe;
2. with Emergency still latched, hold the main physical Emergency button for approximately 3 seconds to create the clear request;
3. **release the physical button**;
4. the Director displays **EMERGENCY CLEARANCE REQUESTED** and **Clear Emergency Stop?**;
5. select **CONFIRM CLEAR** only if the area is safe;
6. if the physical input is still asserted, the P4 rejects clearance;
7. after accepted clearance, verify Emergency state clears on the Director/P4 before restarting any show operation.

The clear request has a limited validity window. If the request times out, start the authorised clear process again rather than trying to bypass it.

## 16.6 Clear does not resume the show

An accepted clear:

- stops P4 Emergency audio;
- releases the P4 Emergency latch;
- returns GPIO24 signage to the normal green-locator pattern;
- returns the P4 GPIO23 show-pixel line to blackout/safe idle;
- does not automatically restart interrupted pixel effects;
- leaves an interrupted running timeline paused until another deliberate show command;
- leaves the Audio Node in safe idle rather than resuming the old file.

**CLEAR EMERGENCY does not mean RESUME SHOW.**

## 16.7 If Emergency will not clear

Check, in this order:

1. Is there still a real-world reason to remain in Emergency? If yes, stop troubleshooting and handle it.
2. Is the physical button/input still asserted? The P4 will reject clearance.
3. Was a valid clear request created and confirmed within its timeout?
4. Is the Director/Communications link available to present the normal confirmation path?
5. Check Diagnostics/System Logs for the rejection reason.

Do not defeat or bypass the Emergency input to restore a show.

## 16.8 Emergency after power loss/reboot

The current P4 latch variable is not proven to persist a previously latched momentary-button Emergency through full power removal. Do **not** assume either persistence or automatic clearance is a certified safety behaviour.

After any power interruption during an incident, treat the attraction as **not ready** until the physical situation and Showduino state have both been re-checked.

## 16.9 Locate Director

The approved production design is:

- first press: Emergency immediately;
- continue holding that same press for 8 seconds: additionally Locate Director;
- Locate wakes/holds the Director display and flashes its ambient LEDs;
- first deliberate Director touch acknowledges Locate only;
- Emergency remains latched.

**This exact behaviour is not implemented at the baseline SHA and is therefore not a normal operator feature in this manual revision.**

---

# 17. Outputs and pixels

## 17.1 P4 Show Pixel Line

The P4 local Show Pixel Line supports up to 16 segment slots in the current engine. Each segment can have a range, effect, primary/secondary colour, brightness, speed, intensity, randomness, direction and duration.

The shared current FX vocabulary includes:

`OFF/BLACKOUT`, `SOLID`, `FADE_IN`, `FADE_OUT`, `PULSE`, `BREATHE`, `FLICKER`, `CANDLE`, `FIRE`, `LIGHTNING`, `STROBE`, `RANDOM_STROBE`, `CHASE`, `BOUNCE`, `COMET`, `WIPE`, `REVERSE_WIPE`, `BUILD`, `SPARKLE`, `TWINKLE`, `GLITCH`, `WARNING`, `PORTAL`, `RAINBOW`, and `CUSTOM_SEQUENCE`.

These are current engine/commissioning capabilities. Persistent production-file PIXEL cue support is not yet complete.

## 17.2 Outputs browser page

The current **Outputs** page includes commissioning controls for the local Show Pixel Line and Audio Node, plus signage/Emergency state information.

While Emergency is active, normal pixel controls must not override the Emergency state.

## 17.3 Pixel hardware acceptance

The repository still requires physical commissioning of the P4 pixel paths. Before a production installation, validate:

- configured pixel count;
- logic level/buffer;
- series resistor;
- supply current and voltage drop;
- power injection;
- common ground;
- all-white Emergency current demand;
- clear-to-blackout/signage behaviour.

---

# 18. Audio

## 18.1 System/safety audio

P4 system audio uses the onboard ES8311 path for Showduino sounds such as boot, Emergency, beep/tone, error, accepted and complete.

If a system WAV is missing or invalid, the Show Engine must continue operating rather than treating audio playback as the safety mechanism.

## 18.2 Attraction/programme audio

Attraction audio is produced by the specialist **Audio Node** from its local SD card.

Current playback support is WAV PCM. MP3, Ogg and FLAC are planned rather than current supported formats.

Current Audio Node operations include play, loop, stop, pause, resume, volume, fade and duck/unduck.

## 18.3 Audio Node Emergency behaviour

Emergency:

- stops/closes programme playback;
- mutes the codec/amplifier path;
- cancels fade/duck state;
- rejects new attraction playback;
- does not auto-resume after clear.

## 18.4 Standalone versus Show-controlled

A supported specialist Node can have a local standalone WebUI when the P4 has not granted Showduino ownership.

When a Node is **SHOW_CONTROLLED**, its local WebUI becomes status/diagnostics-oriented and the firmware rejects local theatrical actions that would fight the P4.

---

# 19. Specialist Nodes

Node support at this release candidate is intentionally described by maturity rather than by wish list.

## 19.1 Audio Node

Implemented in software and part of the active architecture; still requires final hardware acceptance. Programme audio is its primary role.

## 19.2 S3 Lamp Node

Active specialist-node firmware with confirmed principal physical pins. It can operate as a Showduino-controlled or standalone interactive lamp device. Some optional sensor details remain subject to commissioning.

## 19.3 C3 Pixel Node

Software/routing exists and uses the same Showduino FX vocabulary, but physical hardware acceptance remains required.

## 19.4 Wireless Emergency Node

Software exists, but its GPIO choice remains unconfirmed and the accessory is not physically validated at this baseline.

Absolute rules:

- a wireless Emergency Node may **assert** the global P4 Emergency latch;
- it must **never clear** the global latch;
- button release, node reboot or local WebUI must not clear P4 Emergency;
- update/commission Emergency Nodes one at a time;
- an offline Emergency Node is reported as a safety-node fault/warning rather than automatically asserting global Emergency.

The wireless Emergency Node's local NC/mushroom behaviour is separate from the main Showduino momentary button.

## 19.5 MOSFET Node

Planned, not currently a finished product feature.

## 19.6 Relay Node

Legacy/retired direction. Do not specify new installations around the old Relay Node merely because historical firmware remains in the repository.

---

# 20. Plug-in Bus

The P4 Plug-in Bus is a short 3.3 V I²C expansion bus for compatible local modules.

Key technical limits:

- 3.3 V logic;
- SDA GPIO7;
- SCL GPIO8;
- current default bus speed 100 kHz;
- keep cabling short; I²C is not an industrial fieldbus or robust hot-plug bus;
- never pull SDA/SCL to 5 V;
- unknown I²C devices may be detected without being assigned an operational role.

Showduino separates **device identity** from **configured role**. Detecting an I/O expander does not automatically decide whether it is an input or output device.

The Plug-in Bus is an owner/technical-user feature; routine operators should not alter bus wiring during operation.

---

# 21. Storage and productions

## 21.1 P4 production layout

Current persistent productions are stored under:

```text
/showduino/productions/<production-id>/
    manifest.json
    timeline.json
```

## 21.2 Limits in current persistent format

Current production format v1 is bounded and validates file size, cue count, duplicate IDs, time order, path traversal and supported cue types.

At this baseline, persistent timeline cues are limited to internal `TEST`/`LOG` use. This is a release-candidate architecture foundation rather than the final mixed-output customer format.

## 21.3 SHDO v2

SHDO v2 is Showduino's active authoring/interchange contract. The Show Engine does not treat an arbitrary authoring document as executable truth. Deployment/compilation must produce a runtime representation the P4 supports.

## 21.4 SD removal

A timeline already loaded into RAM may continue if the card is removed, but storage operations and SD-streamed media can fault separately.

Do not remove storage during normal attraction operation unless following an explicit service procedure.

---

# 22. Diagnostics and status

Start with the user-facing status surfaces rather than developer tools.

## 22.1 Director

Use:

- **HOME** for overall health;
- **NODES** for specialist-node state;
- **DIAGNOSTICS** for link/system information;
- **SYSTEM LOGS** for recent events/rejections.

## 22.2 Browser

Use:

- **Home** — overall Comms/P4/Director summary;
- **Live** — current authoritative show runtime;
- **Devices** — current device/Node/Plug-in Bus information;
- **Network** — Showduino AP, venue Wi-Fi and P4 network diagnostics;
- **System** — component/system/storage/log/update information.

If the P4 becomes unavailable while the Communications S3 remains alive, the browser can still be served by the S3. P4-owned controls/state must then show unavailable/offline rather than inventing a value.

---

# 23. Software updates

## 23.1 Current update scope

At this baseline, **only the Communications Controller supports self-OTA installation**.

There is no system-wide OTA update button that safely updates every Showduino component.

P4, Director, Audio, Lamp, Pixel and Emergency Node firmware remain service/USB-update components in this phase.

## 23.2 Communications update prerequisites

Comms self-OTA requires, among other software gates:

- the candidate is for the correct Comms hardware;
- the candidate is newer than installed firmware;
- operator confirmation;
- P4 alive;
- no running show;
- no global Emergency;
- P4 maintenance authorisation;
- venue/home Wi-Fi with Internet access for the GitHub download.

Do not start an update during guest operation.

## 23.3 Update process

Current intended operator flow is approximately:

1. connect Showduino to venue/home Wi-Fi from **Network**;
2. open the software/update surface;
3. select **Check for Updates**;
4. confirm the Communications candidate is newer than installed;
5. select **Update Comms** / **INSTALL UPDATE** when the system is idle and safe;
6. allow Communications to reboot;
7. wait for the Director/browser link to reconnect;
8. verify P4/Comms/Director health before returning the attraction to service.

During a Comms reboot, the P4 remains the show/safety authority. Wireless Nodes temporarily lose the transport bridge. The hardwired P4 Emergency input remains independent of the Comms reboot.

## 23.4 OTA integrity and limitations

The implemented Comms updater streams the image into the inactive slot, checks SHA-256 and runs a post-boot health gate with software rollback.

The current release-manifest system is **not cryptographically signed**, and the HTTPS client configuration does not itself prove publisher authenticity. Do not describe the updater as a signed secure-boot distribution system.

Physical normal-update and failed-health rollback acceptance still needs sign-off for the current release candidate.

---

# 24. Routine operation

A typical operating cycle should be deliberately boring:

1. complete the pre-opening checklist;
2. power and confirm the P4, Comms and Director reach healthy states;
3. confirm required Nodes are online;
4. load the correct production;
5. perform a controlled output/audio check before guests enter;
6. use the Director **LIVE** page for operation;
7. respond to Emergency/fault indications before attempting to continue;
8. after the final performance, stop the show and follow the installation's shutdown procedure.

Do not use firmware updates, wiring changes, Node commissioning or pixel-count changes during live guest operation.

---

# 25. Troubleshooting

## 25.1 Director shows CONNECTION LOST

Check whether the failure is:

- Director ↔ Communications radio link;
- Communications ↔ P4 UART link;
- P4 itself.

Do not assume Internet loss caused it. The Director link and venue Internet are separate states.

## 25.2 Browser loads but P4 controls show OFFLINE

The Communications Controller and its WebUI can be alive while the P4 link is unavailable.

Do not repeatedly send commands. Check the P4 power/state and Comms↔P4 connection. If a show may already be running, avoid an unplanned P4 power cycle until the attraction is in a safe state.

## 25.3 Venue Wi-Fi is offline

A loaded show should not require it. Local Showduino operation remains the priority. Internet update checking will not work until connectivity returns.

## 25.4 Audio Node is offline

Programme/attraction audio is unavailable. The P4 system speaker is not a programme-audio fallback.

Check Node power, storage and ownership/link status. Do not convert a system-audio test into an attraction playback workaround.

## 25.5 Pixels are all bright white

Treat this as an Emergency indication first. Do not begin ordinary colour/FX troubleshooting until authoritative Emergency state is confirmed clear.

## 25.6 Show pixels remain black after Emergency clear

That is expected current behaviour. The P4 show-pixel line returns to safe blackout and interrupted FX do not auto-resume.

## 25.7 Production will not start

Check:

- Emergency is clear;
- a valid production/timeline is loaded;
- maintenance/update mode is not active;
- P4 is online;
- the runtime is in a valid start state;
- required Nodes are available for the intended show.

## 25.8 Emergency clear is rejected

Likely current reasons include:

- no valid pending physical clear request;
- physical Emergency input still asserted;
- clear-request timeout expired.

Resolve the physical condition and repeat the authorised procedure. Do not bypass the input.

## 25.9 Node local controls say CONTROLLED BY SHOWDUINO

This is expected when the P4 has granted ownership. Use Showduino's operator/commissioning interface rather than fighting the Node locally.

## 25.10 No SD card

Some already-loaded RAM behaviour may continue, but persistent production/storage/media functions can be unavailable. Treat SD faults according to the production's actual asset dependencies.

---

# 26. Technical installation notes

This section is for owners/technical users, not routine operators.

## 26.1 Main communication path

```text
Director ESP32-S3
  → ESP-NOW
Communications ESP32-S3
  → UART 115200 8N1
P4 Show Engine
```

Current dedicated UART wiring at board level:

- Comms GPIO17 TX → P4 GPIO4 RX
- Comms GPIO18 RX ← P4 GPIO5 TX
- common ground

## 26.2 Local P4 resources

Current principal Showduino assignments include:

- GPIO7/8 — Plug-in I²C bus / onboard ES8311 control;
- GPIO23 — theatrical Show Pixel Line;
- GPIO24 — Emergency/designated-signage pixels;
- GPIO25 — hardwired main Emergency input;
- onboard ES8311/NS4150B path — system/safety audio;
- SD interface — persistent storage;
- onboard C6 — reserved/unused in current product path.

Do not repurpose reserved pins simply because they are physically accessible on a development board.

## 26.3 Pixel electrical standard

For final installations:

```text
P4 GPIO → appropriate 5 V-compatible logic buffer → 470 Ω → pixel DIN
```

Use an external 5 V pixel supply sized for the installed count and worst-case white output, common grounds and suitable power injection.

## 26.4 Plug-in Bus

Use 3.3 V only on SDA/SCL and keep the bus short. Configure device role explicitly rather than relying on an I²C address guess.

---

# 27. Current RC capability summary

| Area | Current status at this manual baseline |
|---|---|
| P4 authoritative runtime | Implemented |
| Director operator UI | Implemented, current page UI active; OS2 disabled |
| Communications S3 transport/WebUI host | Implemented |
| Local Showduino SoftAP | Implemented; production credential provisioning gap remains |
| Optional venue Wi-Fi | Implemented; hardware acceptance still required |
| P4 system/safety audio | Implemented foundation; physical acceptance required |
| Audio Node | Implemented software; hardware acceptance required |
| S3 Lamp Node | Active; principal pins confirmed |
| P4 segmented Show Pixels | Implemented; hardware acceptance required |
| C3 Pixel Node | Software/routing active; hardware acceptance required |
| Wireless Emergency Node | Software implemented; GPIO/physical acceptance incomplete |
| Persistent production store | Implemented foundation; format v1 TEST/LOG cue limitation |
| SHDO v2 | Active authoring/interchange contract |
| Full integrated attraction/scene Studio workflow | Partial / not yet a finished on-device workflow |
| Comms self-OTA | Implemented in software; physical proof required |
| System-wide OTA | Not implemented |
| MOSFET Node | Planned |
| Relay Node | Legacy/retired |
| DMX production control | Parked / not current product scope |
| Approved 8-second Emergency-button Locate | Not implemented at this SHA |

---

# 28. Glossary

**Authoritative** — the component whose state is the official system truth. For live show/safety state, this is the P4 Show Engine.

**Communications Controller / Comms** — the ESP32-S3 transport and local WebUI host between Director/browser/Nodes and P4.

**Director** — the Showduino touchscreen operator interface.

**Emergency latch** — Showduino's software-held Emergency state, which remains active after the main momentary button is released until an authorised clear is accepted.

**ESP-NOW** — the local radio transport currently used between the Director/Nodes and Communications Controller. An operator does not need to configure ESP-NOW directly.

**Node** — a specialist device such as an Audio, Lamp, Pixel or Emergency Node.

**P4 / Show Engine** — the authoritative Showduino runtime controller.

**Production** — an installed runtime show package known to the P4.

**SHDO** — Showduino's versioned production authoring/interchange format contract.

**SoftAP / access point** — the Wi-Fi network created by Showduino itself for local browser access.

**Studio** — Showduino's browser authoring/commissioning family. At this RC baseline, distinguish the active S3 system console from the fuller Studio V4 authoring workflow still being integrated.

**Timeline** — the ordered time-based show actions owned/executed by the Show Engine once loaded.

---

# 29. Manual revision and support baseline

When reporting a fault or comparing behaviour with this manual, record:

- Showduino product version;
- P4 firmware version;
- Communications firmware version;
- Director firmware/release identity if shown;
- affected Node firmware version;
- manual revision;
- repository/firmware release identifier where supplied.

For this manual revision the source baseline is:

```text
Showduino platform: 1.0.0-rc.1
P4 source:           0.6.4 at baseline HEAD
Comms source:        0.5.2 at baseline HEAD
Director manifest:   0.9.7-director (release manifest; current source lacks an equivalent clear BoardConfig version constant)
Repository SHA:      9f304cdc65786c0e8d306987010a97eb5a3ad587
Date:                16 September 2026
```

The committed release manifest still lists P4 0.6.3 and Comms 0.5.1, so the exact repository SHA is intentionally retained as the documentation baseline until release inventory is refreshed.

---

## End of manual

For unresolved implementation, validation and product decisions, see `SHOWDUINO_MANUAL_GAPS.md`. For claim-level traceability, see `SHOWDUINO_MANUAL_SOURCE_MATRIX.md`.
