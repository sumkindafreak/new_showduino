# SHOWDUINO — Pre-Opening Checklist

**Use:** before admitting guests / beginning public operation  
**Manual baseline:** Showduino `1.0.0-rc.1` · repository SHA `9f304cdc65786c0e8d306987010a97eb5a3ad587`  
**Date:** ____________________  **Attraction/production:** ____________________  **Operator:** ____________________

> **People first. Show second. Hardware third.** If any check reveals a safety concern, keep the attraction closed until the physical issue and Showduino state are understood.

---

## 1. Physical inspection

- [ ] Main Showduino enclosure/control equipment is secure and undamaged.
- [ ] Director touchscreen is undamaged and accessible to the operator.
- [ ] Power and data connectors are fully seated.
- [ ] No exposed conductor, damaged cable, overheating, unusual smell or loose connection is present.
- [ ] External output drivers, power supplies and protective devices appear normal.
- [ ] Pixel power supplies/cabling show no sign of overheating or damage.
- [ ] Required SD cards/storage are fitted.
- [ ] Main momentary **Emergency** button moves freely and is not obstructed.
- [ ] Venue fire/evacuation/machinery safety systems required by the installation are available independently of Showduino.

**Stop here if any physical condition is unsafe.**

---

## 2. System power and core health

Current RC test order is P4 → Communications → Director → Nodes.

- [ ] P4 Show Engine powered and reaches ready/healthy state.
- [ ] Communications Controller powered.
- [ ] Director powered.
- [ ] Director ↔ Communications link reports healthy/ready.
- [ ] Communications ↔ P4 link reports online/ready.
- [ ] No unexplained **CONNECTION LOST**, **FAULT** or **DEGRADED** condition remains.
- [ ] Emergency is currently clear before beginning output tests.

Remember: venue Wi-Fi/Internet status is not the same thing as P4 health.

---

## 3. Emergency function check

**Carry this out before guests enter and with the attraction in a controlled test condition.**

- [ ] Confirm no person is in a position where the test can create a hazard/startle risk.
- [ ] Press the main Showduino Emergency button.
- [ ] Emergency asserts promptly after normal input debounce.
- [ ] Release the momentary button.
- [ ] Emergency **remains latched** after release.
- [ ] Director clearly shows the Emergency state.
- [ ] Any running test timeline stops progressing/enters Emergency pause.
- [ ] P4 Show Pixel line changes to bright white where installed.
- [ ] Dedicated signage pixels change to bright white where installed.
- [ ] Attraction Audio Node stops/mutes where installed.
- [ ] Other installed Emergency-capable Nodes enter their defined Emergency state.
- [ ] Normal show controls do not override Emergency.

### Clear

The physical Emergency button never clears Emergency.

- [ ] Release the physical button if it is still held.
- [ ] On the Director Emergency screen, choose **CLEAR EMERGENCY**.
- [ ] Director presents **Clear Emergency Stop?** / clearance confirmation.
- [ ] Select **CONFIRM CLEAR** only after confirming the test area is safe.
- [ ] P4 accepts the clear only with the physical input released.
- [ ] Authoritative Emergency state changes to clear.
- [ ] Signage returns to normal green-locator pattern where installed.
- [ ] P4 show pixels return to blackout rather than the previous effect.
- [ ] Attraction audio does **not** automatically resume.
- [ ] The show does **not** automatically continue merely because Emergency was cleared.

This firmware path is implemented and still requires physical hardware acceptance.

**FAIL:** any Emergency item above fails or behaves differently from the installed/manual baseline.  
**Action:** keep the attraction closed until diagnosed and re-tested.

---

## 4. Required Nodes

Mark N/A only if the production genuinely does not use that device.

- [ ] Audio Node online / N/A.
- [ ] Lamp Node online / N/A.
- [ ] Pixel Node(s) online / N/A.
- [ ] Wireless Emergency Node(s) healthy/linked / N/A.
- [ ] Required Plug-in Bus devices online/configured / N/A.
- [ ] No required Node is showing FAULT, NO STORAGE or unexpected STANDALONE state.

For wireless Emergency Nodes:

- [ ] Each installed station expected for this attraction is visible/healthy.
- [ ] Any offline Emergency Node warning has been investigated before opening.
- [ ] No station is left locally latched/NEEDS REARM.

Wireless Emergency Nodes are **assert-only** and must never be used as a way to clear the P4 Emergency latch.

---

## 5. Production

- [ ] Correct production/show name selected.
- [ ] Correct production loaded on the P4.
- [ ] No unintended maintenance/update mode is active.
- [ ] Runtime state is suitable for a fresh start.
- [ ] A controlled START request is accepted by the P4.
- [ ] Timeline/state progresses as expected.
- [ ] PAUSE works if used by the attraction.
- [ ] STOP works and leaves the attraction in the intended idle/safe state.

**RC note:** persistent production format v1 is not yet the complete mixed-output commercial cue format. Use only the production/deploy path validated for this installation.

---

## 6. Audio

- [ ] P4 system/safety audio path reports expected status / N/A if intentionally unavailable.
- [ ] Audio Node storage is present where attraction audio is required.
- [ ] Correct test asset plays from the Audio Node.
- [ ] Volume is appropriate for the venue.
- [ ] Audio stops correctly.
- [ ] No unexpected clipping, silence, reboot or storage fault occurs.

Do not use the P4 system speaker as a substitute for missing attraction/programme audio.

---

## 7. Pixels / lighting outputs

- [ ] P4 Show Pixel line initialises correctly / N/A.
- [ ] Configured segment ranges match the physical installation.
- [ ] Controlled effect/colour test behaves as expected.
- [ ] No flicker/data corruption caused by wiring or power issues.
- [ ] Dedicated signage line shows the expected normal pattern / N/A.
- [ ] Pixel supplies/cabling remain cool and stable during the intended test load.

If pixels unexpectedly become **all bright white**, check authoritative Emergency state before treating it as a lighting fault.

---

## 8. Browser / network

The local Showduino browser connection is useful but is not required to stay open during a loaded show.

- [ ] Local `Showduino` Wi-Fi is available if browser operation is required.
- [ ] `http://192.168.4.1/` opens the local system console on the current RC network / N/A if installation address has been changed.
- [ ] Browser Home shows expected P4/Comms/Director state.
- [ ] Optional venue Wi-Fi state is correct / N/A.
- [ ] If venue Wi-Fi is unavailable, local P4 operation is still healthy.
- [ ] Internet availability is not being mistaken for Show Engine availability.

Do not start a firmware update during public operation.

---

## 9. Director

- [ ] HOME shows expected overall state.
- [ ] PRODUCTIONS shows/selects the intended production.
- [ ] LIVE reflects P4-confirmed runtime state.
- [ ] NODES shows required specialist Nodes.
- [ ] DIAGNOSTICS contains no unresolved critical fault.
- [ ] SYSTEM LOGS do not show repeated/recent unexplained failures.
- [ ] Touchscreen responds normally.
- [ ] Display brightness/auto-off setting is appropriate for the operator position.

**Locate:** press and hold the main Emergency button for 8 seconds to locate the Director. First touch acknowledges Locate only and does not clear Emergency. Hardware acceptance of this path is still required.

---

## 10. Controlled full test

- [ ] Run one complete controlled cycle of the attraction/production before opening.
- [ ] Audio events occur at expected points / N/A.
- [ ] Pixel/lighting events occur at expected points / N/A.
- [ ] Specialist effects/Nodes respond as expected / N/A.
- [ ] Show reaches completion or stops correctly.
- [ ] No unexpected Node dropout occurs.
- [ ] No unexpected Director/Comms/P4 reconnect occurs.
- [ ] No unexplained fault remains after the test.

---

# OPEN / DO NOT OPEN

- [ ] **OPEN** — all required checks passed and the attraction is ready for operation.
- [ ] **DO NOT OPEN** — one or more required checks failed or remain unexplained.

**Outstanding issue(s):**

______________________________________________________________________________

______________________________________________________________________________

______________________________________________________________________________

**Action taken / person responsible:**

______________________________________________________________________________

______________________________________________________________________________

**Operator signature:** ______________________________  **Time:** ______________

---

## Fault rule

Do not clear a fault merely to make the screen look green. Find the affected layer:

```text
People / physical attraction condition
        ↓
P4 Show Engine authority
        ↓
Communications / Director
        ↓
required specialist Node
        ↓
individual output / asset / wiring
```

Correct the actual fault, then re-run the affected checklist section before opening.
