# MISSION 001L — Emergency storage faults and maintenance recovery

**Status:** the recovery commands are implemented in the P4 firmware and host-tested.
They have **not** been run on hardware. No storage fault has been injected on a real
board. Showduino's emergency function is an entertainment show-control interlock, not
certified safety equipment.

This document replaces the "no software exit" limitation in
`docs/emergency-station-service-recovery.md` (items 2 and 4 of its known limitations).

## 1. Rules every recovery follows

1. **Never silently clear a possibly active emergency.** No recovery command changes the
   latch. After recovery the emergency is still latched and needs a normal, fully
   verified clear (Director or P4 USB, with fresh RELEASED proofs from every station).
2. **Never discard unknown blockers.** The recovered blocker set is every readable stored
   blocker **plus** every provisioned station. It can only grow. If it does not fit, the
   command is refused; nothing is truncated.
3. **Never from the Comms S3.** Every `EMERGENCY:RECOVER:`, `EMERGENCY:KEY:`,
   `EMERGENCY:DIRKEY:`, `EMERGENCY:STATION:` and `EMERGENCY:AUTH:` line is handled before
   any logging and only when it arrives on the **P4's own USB port**. From the UART it
   prints `REJECTED station maintenance command — local USB console only`.
4. **Explicit authority.** Each recovery needs a typed confirmation word that states what
   the person did. Typing it is a statement that the inspection was done.
5. **Outputs stay safe.** While latched, every local output stays in its emergency state
   during and after recovery.
6. **Logged.** Every recovery prints a `MAINTENANCE ...` line to the P4 log. Copy it into
   the event record with the person's name.

## 2. Before any recovery

* Keep the show stopped and every hazardous load isolated (see the acceptance document §3).
* P4 USB console, 115200 baud. Type `EMERGENCY:RECOVER:STATUS`:
  ```
  [ESTOP] RECOVER latched=1 unknownBoot=1 readableBlockers=0 persistDirty=0 counterLoaded=1 keyFault=0 dirFault=0 loopOpen=0
  ```
  | Field | Meaning |
  |---|---|
  | `latched` | emergency is active |
  | `unknownBoot` | the P4 restarted latched but cannot trust its blocker or latch record → §3.1/§3.2 |
  | `readableBlockers` | how many stored blockers could still be read (they will be kept) |
  | `persistDirty` | the last blocker write did not verify; it retries every 5 s → §3.6 |
  | `counterLoaded=0` | the command counter could not be read → §3.5 |
  | `keyFault=1` | station key record unreadable → §3.3 |
  | `dirFault=1` | Director key record unreadable → §3.4 |
  | `loopOpen=1` | the P4 GPIO25 button is pressed |
* Also `EMERGENCY:AUTH:STATUS`, `EMERGENCY:KEY:LIST`, `EMERGENCY:DIRKEY:STATUS`. Save
  the output.

## 3. Fault cases

### 3.1 Missing latch record

| | |
|---|---|
| Detection | Since 001L the P4 writes a marker key (`estop_init`) after its first verified latch write. At boot: latch key missing **and** marker present → the record was lost. |
| Firmware behaviour | Boot prints `latch storage unreadable, corrupt or lost — fail-safe LATCHED, clear blocked` and the recovery hint. Emergency latched, outputs engaged, every clear refused (`unknownBoot=1`). |
| Procedure | §4 (blocker recovery). |
| Re-provisioning | None, unless other records are also lost. |
| Limits | (a) If both the latch key and the marker are lost (for example a full NVS erase), the board is indistinguishable from a brand-new one and boots CLEAR. (b) How ESP32 NVS reports a damaged entry (missing key versus read error) has not been observed on hardware; host test L1 covers the decode only. (c) A board that has never run 001L firmware gets its marker on the first 001L boot (migration); before that, a lost key reads as CLEAR. |

### 3.2 Corrupted or unverified blocker record

| | |
|---|---|
| Detection | Latched at boot and the station blocker record is unreadable, incomplete, or was written but never verified. |
| Firmware behaviour | `unknownBoot=1`; clear refused `STATION_STORAGE`; nothing is written over the unknown record. The readable part (if the record decodes) is kept in RAM for recovery. |
| Procedure | §4. |
| Not latched at boot | No recovery needed: the next change writes a fresh verified set. |

### 3.3 Unreadable station key record

| | |
|---|---|
| Detection | `keyFault=1`; boot log `station key record unreadable — clear blocked`. |
| Behaviour | No station can authenticate; every clear is refused. ASSERT still latches. |
| Procedure | Physically inspect every station. `EMERGENCY:KEY:RESET` (accepted only while the record is unreadable). Then re-provision **every** station: on each C3 `PROVISION NEW ESTOP-nn`, paste the printed line into the P4 (acceptance document §6.3). Then clear normally. |
| Re-provisioning | All stations. |

### 3.4 Corrupted Director credential

| | |
|---|---|
| Detection | `dirFault=1`; Director clears refused. |
| Behaviour | No Director message is accepted. The P4 USB clear still works (with station proofs). |
| Procedure | `EMERGENCY:DIRKEY:RESET` (only while unreadable), then `PROVISION DIRECTOR NEW DIR-nn` on the Director and paste the line into the P4. If enrolled while latched the Director is restricted until one P4 USB clear has been accepted. |
| Director side | If the Director's own record is unreadable it prints `key record unreadable — enrol again`; do the same enrolment. |
| Re-provisioning | The Director. |

### 3.5 Damaged replay-counter storage (P4)

| | |
|---|---|
| Detection | `counterLoaded=0`; boot log `station counter unreadable — station commands held, clear blocked`. |
| Behaviour | No episode can be created, no station command (ACK/GCL) is sent; every clear is refused. |
| Procedure | `EMERGENCY:RECOVER:COUNTER:COUNTER-LOST`. The P4 writes and reads back a new counter starting at `0xC0000000` and prints `RECOVER:COUNTER OK`. |
| Why this is safe | Stations accept a command only when its counter is **higher** than the last one they stored. Restarting high can only cause a station to *reject* a command, never to accept a replay. If a station then refuses commands (it stays latched after a clear), re-provision that station. |
| Counter key missing entirely | Treated as a first boot (counters restart at 1). Stations then reject P4 commands, so latched stations do not return to NORMAL after a clear (fail closed). Re-provision those stations. Not fixed in 001L: there is no marker for this key. |
| Re-provisioning | Only stations that reject commands afterwards. |

### 3.6 NVS exhaustion and failed flash writes

| | |
|---|---|
| Detection | `persistDirty=1`, `[ESTOP] WARNING: latch ACTIVE could not be verified`, `KEY:SET ... FAILED`, clear refused `STATION_STORAGE` or `PERSIST_FAILED`. |
| Behaviour | Every write is read back. A failed write never releases anything: the RAM latch stays active and clear stays refused. Writes retry every 5 s. |
| Procedure | Transient: wait and retry. Persistent: the NVS partition is full or worn. There is **no in-place fix**. Service action: (1) physically inspect every station and the installation; (2) isolate all show power; (3) erase flash and reflash the P4; (4) re-provision every station and the Director; (5) run acceptance sections A and C again before reconnecting loads. **After an erase the P4 boots CLEAR** — that is why show power must stay isolated until step 5 is complete. |
| Re-provisioning | Everything on the P4. |
| Hardware confirmation needed | NVS full / wear-out behaviour has not been observed on hardware. |

### 3.7 C3 station faults

| Fault | Behaviour | Procedure |
|---|---|---|
| C3 key record unreadable | `key record unreadable — station cannot authenticate`; the station still asserts but blocks clear | `PROVISION NEW ESTOP-nn` (or `PROVISION SET`) and re-enter the key on the P4 |
| C3 counter not stored | provisioning prints `FAILED — counter not stored` | repeat provisioning; replace the C3 if it persists |
| C3 lost or broken during an emergency | blocks clear | inspect the location by hand; replace with the same ID (`PROVISION NEW` on the replacement + `KEY:SET` on the P4), or retire: `EMERGENCY:KEY:REVOKE:ESTOP-nn` then `EMERGENCY:STATION:DISMISS:ESTOP-nn` |

## 4. Blocker recovery procedure (cases 3.1 and 3.2)

1. **Isolate.** Show stopped, hazardous loads isolated. Emergency stays latched.
2. **Inspect every emergency station by hand.** Record ID, location, button state
   (released), and that it is powered. Include any station listed by
   `EMERGENCY:AUTH:STATUS` or remembered from the installation plan, provisioned or not.
3. **Release the P4 GPIO25 button.** `loopOpen` must be 0.
4. Type exactly:
   ```
   EMERGENCY:RECOVER:BLOCKERS:INSPECTED-ALL-STATIONS
   ```
5. Expected reply:
   ```
   [ESTOP] RECOVER:BLOCKERS OK — <n> station(s) must prove RELEASED; record stored; emergency still LATCHED
   ```
   Refusals and what they mean:
   | Reply | Meaning |
   |---|---|
   | `REFUSED_NOT_LATCHED` | nothing to recover |
   | `REFUSED_NOT_NEEDED` | the record is already known; use the normal clear |
   | `REFUSED_CONFIRMATION` | wrong or missing confirmation word |
   | `REFUSED_BUTTON_ACTIVE` | GPIO25 pressed, or its state not yet known (wait 1 s after boot) |
   | `REFUSED_OVERFLOW` | more stations than the table holds; retire stations (§3.7) first |
   | `record NOT stored` | write failed; clear stays blocked and it retries (§3.6) |
6. Every listed station must now come online and prove RELEASED. Then clear normally
   (Director or `EMERGENCY:CLEAR` on the P4 USB console).
7. A station that cannot be brought back must be retired (§3.7) after inspection.
8. Record the `MAINTENANCE blocker recovery` log line, your name and the inspection notes.

What the command does: adds every provisioned station (and every readable stored
blocker) as "pressed, offline, unverified", clears the unknown flag, writes and verifies
the new blocker set, and lets the latch record be rewritten as ACTIVE on the next 5 s
reconcile. It never writes CLEAR. Host tests: L3 (gate), L4 (set), L7a–L7g (end to end in
the integration harness, including "a silent station still blocks").

Known gap: an **unprovisioned** station that pressed before the record was lost cannot
be recovered by software — only the physical inspection in step 2 covers it.

## 5. What must be restored after each case

| Case | Station keys | Director key | Counter | Physical inspection |
|---|---|---|---|---|
| 3.1 / 3.2 blocker or latch lost | — | — | — | **all stations** |
| 3.3 station keys unreadable | **all** | — | — | all stations |
| 3.4 Director credential | — | **yes** | — | — |
| 3.5 counter unreadable | only stations that then reject commands | — | recovered | — |
| 3.6 NVS full / erase | **all** | **yes** | reset | **all stations + installation** |
| 3.7 C3 faults | that station | — | — | that station |

## 6. Blockers that need hardware confirmation

1. Real NVS behaviour on damaged entries (does a damaged key read as missing, as a read
   error, or take the marker with it?).
2. Loss of both the latch key and the marker boots CLEAR (§3.1 limit a).
3. Counter key missing entirely restarts at 1 (§3.5); fail closed but needs
   re-provisioning.
4. NVS full / worn behaviour (§3.6).

None of these is worked around with a bypass. If a case is met in the field that this
document does not cover, keep the emergency latched and the show isolated, and escalate.
