# SHOWDUINO — Quick Start

**For:** operators, attraction owners and first-time users  
**Manual baseline:** Showduino `1.0.0-rc.1` · repository `main` SHA `9f304cdc65786c0e8d306987010a97eb5a3ad587` · 16 September 2026  
**Status:** Release-candidate quick start. Read the full User Manual before putting a new installation into public operation.

---

## The one thing to remember

```text
Show Engine decides.
Communications transports.
Director commands and displays.
Nodes act.
```

The P4 Show Engine is the authority. The browser and Director are operator interfaces; an already loaded show is intended to keep running without an open browser, without the Director and without Internet access.

---

# 1. Before you switch on

- Check the main unit, Director, cables and Nodes for damage.
- Confirm the correct power supplies are connected.
- Keep uncommissioned/high-power external effects isolated for first setup.
- Confirm the P4 SD card is fitted if the installation requires stored productions/assets.
- Check the main momentary **Emergency** button moves freely.
- Confirm external pixel power supplies and grounds are correctly installed.

**WARNING:** Showduino show-control Emergency behaviour does not replace any fire, evacuation, machinery or other certified safety system required by the venue.

---

# 2. Power on

For the current RC architecture, use the repository's test sequence:

1. **P4 Show Engine**
2. **Communications Controller**
3. **Director touchscreen**
4. configured specialist Nodes

Allow the system to discover its links and Nodes. Do not judge readiness from the touchscreen being lit alone.

Before running a show, confirm:

- Director/Communications link is healthy;
- Communications/P4 link is healthy;
- P4 is ready;
- Emergency is clear;
- required Nodes are online;
- the correct production is loaded.

---

# 3. Connect a phone, tablet or computer

Current RC bench network:

```text
Wi-Fi:    Showduino
Password: showduino
Address:  http://192.168.4.1/
Studio:   http://192.168.4.1/studio/
```

`showduino` is currently a **bench credential**, not a finished public-venue provisioning policy. Do not treat it as an acceptable permanent public-installation password.

The local browser interface does not require Internet access.

---

# 4. Know the two browser surfaces

The current local **system console** provides:

- Home
- Productions
- Live
- Outputs
- Devices
- Network
- System
- Settings

The fuller Studio/SHDO authoring experience is still being integrated. At this baseline, do **not** assume the complete `New Attraction → Scene → Devices → Timeline → Deploy` commercial journey is finished just because Studio source/design work exists.

For a first practical RC test, use an already installed production or the documented commissioning/deploy path rather than inventing files manually.

---

# 5. Load and run a production

On the Director, the main current pages include **HOME**, **PRODUCTIONS**, **SHOW DETAILS**, **LIVE**, **NODES**, **DIAGNOSTICS**, **SETTINGS** and **SYSTEM LOGS**.

A normal run is:

```text
PRODUCTIONS
    ↓
select/load the intended production
    ↓
confirm it is loaded
    ↓
LIVE
    ↓
START
    ↓
watch P4-confirmed RUNNING state
```

Loading/deploying does not automatically start the show.

Use **PAUSE**, **RESUME** and **STOP** only when the P4-confirmed state makes the requested action valid.

---

# 6. If you press Emergency

The main Showduino Emergency control is a **momentary** button with a **software latch**.

A valid press causes Emergency to assert immediately after debounce. Releasing the button does **not** clear it.

Current implemented effects include:

- the running timeline pauses;
- the Director shows **EMERGENCY** / **SHOW STOPPED**;
- P4 show pixels and designated-signage pixels are forced bright white;
- attraction Audio Node playback stops/mutes;
- normal show actions cannot override the Emergency state.

**People first. Deal with the incident before trying to recover the attraction.**

## Clearing Emergency on this exact RC baseline

The current firmware uses a **3-second physical hold to request clearance**, followed by a separate Director confirmation. This procedure is expected to change because the approved production design now assigns an 8-second hold to Director Locate.

For this exact baseline only:

1. Resolve the real-world incident and confirm the attraction is safe.
2. Hold the main Emergency button for about 3 seconds to create the clear request.
3. Release the physical button.
4. On the Director, read **Clear Emergency Stop?** carefully.
5. Select **CONFIRM CLEAR** only when safe.
6. Verify authoritative Emergency state changes to clear.

If the physical input is still asserted, the P4 rejects the clear.

**CLEAR EMERGENCY DOES NOT RESUME THE SHOW.**

After clear, local show pixels remain black and interrupted attraction audio does not resume. A previously running show may remain paused until the operator deliberately stops/restarts or resumes it according to the venue's recovery procedure.

---

# 7. Director Locate — RC status

The approved production behaviour is:

```text
Emergency press → Emergency immediately
same continuous hold reaches 8 seconds → Locate Director as well
first Director touch → acknowledge Locate only
Emergency remains latched
```

That exact behaviour is **not implemented at this repository baseline**. Do not train operators on the temporary current multi-press locator gesture. See `SHOWDUINO_MANUAL_GAPS.md`.

---

# 8. Venue Wi-Fi is optional

The Showduino local access point is the normal direct browser route.

If you want optional home/venue Wi-Fi:

1. open **Network**;
2. select **Scan** or enter the **SSID**;
3. enter the password;
4. select **Connect**;
5. confirm the Showduino AP remains available.

Use **Disconnect** to return to AP-only while keeping saved credentials. Use **Forget** to remove saved venue credentials.

If venue Wi-Fi or Internet disappears, do not assume the P4 Show Engine is down. A loaded show is intended to continue locally.

---

# 9. Before guests enter

Run the separate `SHOWDUINO_PRE_OPENING_CHECKLIST.md`.

At minimum verify:

- Emergency press/latch/clear behaviour;
- correct production loaded;
- P4, Comms and Director healthy;
- required Nodes online;
- programme audio available if required;
- show pixels/signage behaving correctly;
- no unexpected fault/offline indication;
- a controlled test run completes correctly.

---

# 10. If something looks wrong

**Browser works but P4 says OFFLINE:** Comms is alive; the Show Engine link is not. Do not confuse the two.

**Internet is OFFLINE:** update checking may fail; the local show should not require Internet.

**Audio Node OFFLINE:** programme audio is unavailable; the P4 system speaker is not a fallback.

**All pixels are bright white:** treat it as Emergency first, not as a lighting fault.

**Show pixels are black after Emergency clear:** expected current safe behaviour; effects do not auto-resume.

**Node says CONTROLLED BY SHOWDUINO:** expected while the P4 owns it; use Showduino controls.

**Emergency will not clear:** check the physical button/input has been released and a valid clear request was created. Do not bypass the input.

---

## RC limitation summary

At this manual baseline:

- P4 runtime, Comms transport/WebUI and Director operator UI are implemented;
- Comms self-OTA is implemented in software, but system-wide OTA is not;
- several hardware paths still require formal physical acceptance;
- full mixed-device persistent production authoring is not complete;
- approved 8-second Locate behaviour is not yet implemented;
- the product remains `1.0.0-rc.1`, not final `v1.0.0`.

For complete operating and technical information, use `SHOWDUINO_USER_MANUAL.md`.
