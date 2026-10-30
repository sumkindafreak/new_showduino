# SHOWDUINO — Quick Start

> **30 October 2026 status:** On 29 October the owner reported Showduino's **first real combined audio-and-visual show playback** on physical hardware. This supersedes statements that no integrated show has ever played. It is **not** a signed pass of the 22-second all-node LOAD → START → COMPLETE fixture or mid-show Emergency repeat. Owner-reported working hardware includes Audio Node WAV playback and P4/Audio Node pixel output. Exact flashed firmware versions and complete timing logs remain unrecorded here. Previous Director Core 1 LoadProhibited resets and Audio microphone CALIBRATE/threshold acknowledgements remain unresolved until retested. Distinguish current main, unmerged PRs and flashed firmware. The 20 October source baseline remains historical. See [30 October reconciliation](SHOWDUINO_FINISH_LINE_STATUS_2026-10-30.md).

**Documentation review:** 20 October 2026 · firmware source `f4c9aeaabdcf939417f9891fb01d98e1e5d8aec2` · [review scope and evidence](SHOWDUINO_DOCUMENTATION_REVIEW.md). Source review does not establish physical acceptance.

**For:** operators, attraction owners and first-time users  
**Manual baseline:** Showduino `1.0.0-rc.1` · repository `main` SHA `f4c9aeaabdcf939417f9891fb01d98e1e5d8aec2` · 20 October 2026  
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

The fuller Studio/SHDO authoring experience is substantially more capable than earlier RC wording, but persistent mixed-device production storage on the P4 is still limited. At this baseline, do **not** assume every authored timeline cue is already a finished persistent on-device show.

For a first practical RC test, use an already installed production or the documented commissioning/deploy path rather than inventing files manually.

---

> **24 October finish-line check:** The existing [22-second all-node acceptance production](https://github.com/sumkindafreak/new_showduino/blob/integration/pr27-pr28-current-main/examples/productions/ALL_NODES_BENCH_TEST.md) is on unmerged PR #29, **not** current `main`. The `main` [first-audio-test.shdo](../../examples/productions/first-audio-test.shdo) is a smaller 30-second audio-only smoke test (22 seconds of WAV playback), not an all-node pass. Review the [current main/bench audit](SHOWDUINO_FINISH_LINE_AUDIT_2026-10-24.md), record installed firmware versions and do not mark either show accepted until a physical run and Emergency repeat pass. Audio Node GPIO5 status/KEY6 disable and default-ten-pixel behaviour are draft PR #40 only; current `main` needs explicit GPIO22 pixel COUNT/INIT commissioning.

> **29 October first-show update:** The owner has now successfully played a **real show with audio and visual effects together on hardware**. This supersedes any implication that Showduino has never played a show. The production identity, exact installed firmware, complete/terminal state, repeatability and Emergency interruption/recovery have **not** been logged as a formal acceptance pass. Preserve the working configuration, repeat that same show with P4-confirmed COMPLETE, then run its Emergency interruption/clear test. The separate PR #29 22-second all-node fixture remains formally `NOT RUN`; do not equate that record with the achieved first-show milestone. See the [29 October audit](SHOWDUINO_FINISH_LINE_AUDIT_2026-10-24.md).

## Website Studio → installed production

1. Open [Website Studio](https://show-duino.com/studio.html). Create a show, use **Create my first show** if helpful, then build and preview a simple Pixel or Audio cue. Save locally and export a `.shdo` backup. Browser preview does not operate the hardware.
2. In **Deploy**, save the **Communications S3** LAN address (not the P4 address). Use the same local network. A saved IP, a creator/cloud label or a lit screen does not verify the P4 or any Node.
3. On the public HTTPS website, **Prepare for P4** prepares a file. Use **Open Local Console** for the S3-hosted interface. Browser private-network/mixed-content restrictions may prevent website verification even when the local console works.
4. In that console, open **Productions → Persist SHDO v2 to P4 SD**, choose the exported file and select **Persist to SD**. Wait for the P4 result. Persistence compiles only supported cues; a rejected file is not installed. Copying an editable SHDO onto SD is not this deployment process.
5. Persisting does **not** load or start the production. Select the intended production and **Load**, verify P4-confirmed loaded state, then use the Director **LIVE → START** when the test area is ready.
6. Confirm actual Node outputs, stop behaviour and Emergency/recovery on isolated test loads. Save the firmware identifiers and physical test results before guest operation.

Public Studio, the embedded `/studio/` snapshot and the S3 system console can be different builds. The local console's **Persist to SD** workflow writes SD; `/api/studio-timeline` is a separate temporary RAM commissioning path and does not persist a production.

### Accounts, membership and indicators

A free Showduino ID provides community/account access; optional Starter, Pro and Studio memberships are proposed, with no published feature/price promise or payment activation. Hardware-owner entitlements are separate and require confirmed terms; registering a browser inventory entry does not activate access or prove a device is connected. Existing Studio authoring tools remain available without paid membership.

Website hardware status starts **HARDWARE NOT VERIFIED**. **S3 CONNECTED** requires a ready Showduino capability response at the configured/local address; it establishes S3 reachability only. **P4 · NOT VERIFIED** does not establish runtime health. Check authoritative P4 and required Node states in the Director/local console. **LOCAL** describes saving, and **CREATOR · TESTING** describes browser mode.

---

# 5. Load and run a production

On the Director, the main current pages include **HOME**, **PRODUCTIONS**, **SHOW DETAILS**, **LIVE**, **NODES**, **AUDIO NODE**, **LAMP NODE**, **DIAGNOSTICS**, **SETTINGS** and **SYSTEM LOGS**.

**Settings → Display → CALIBRATE** realigns the touchscreen. A valid calibration is stored on the Director and loads at boot. If none is stored, factory mapping is used.

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

## Clearing Emergency

The physical Emergency button never clears Emergency.

1. Resolve the real-world incident and confirm the attraction is safe.
2. Release the physical Emergency button if it is still held.
3. On the Director Emergency screen, choose **CLEAR EMERGENCY**.
4. Read **Clear Emergency Stop?** carefully.
5. Select **CONFIRM CLEAR** only when safe.
6. Verify authoritative Emergency state changes to clear.

If the physical input is still asserted, the P4 rejects the clear.

**CLEAR EMERGENCY DOES NOT RESUME THE SHOW.**

After clear, local show pixels remain black and interrupted attraction audio does not resume. A previously running show may remain paused until the operator deliberately stops/restarts or resumes it according to the venue's recovery procedure.

This firmware path is implemented and still requires physical hardware acceptance.

---

# 7. Director Locate

```text
Emergency press → Emergency immediately
same continuous hold reaches 8 seconds → Locate Director as well
first Director touch → acknowledge Locate only
Emergency remains latched
```

If the Director cannot be found, keep holding the same Emergency button for 8 seconds. The Director wakes its screen, keeps the backlight on, and flashes its ambient lights until you touch the screen once. That touch acknowledges Locate only.

Hardware acceptance of this path is still required. See `SHOWDUINO_MANUAL_GAPS.md`.

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
- approved 8-second Locate and Director-only clear workflows are implemented in firmware but still require hardware acceptance;
- the product remains `1.0.0-rc.1`, not final `v1.0.0`.

For complete operating and technical information, use `SHOWDUINO_USER_MANUAL.md`.
