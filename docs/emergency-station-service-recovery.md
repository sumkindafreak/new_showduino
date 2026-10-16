# Wireless Emergency Node — persistent blocker and service recovery

Status: **software implemented on branch `fix/emergency-persistent-latch`. Not compiled for target, not hardware-tested.**

## What persists
- The P4 stores the set of wireless stations whose last valid report says **pressed** (`input_open=1`), with their stable ID (`ESTOP-NN`) and MAC. Stored in NVS (`showduino` / `estop_blk`) as a checksummed string, written only when that set changes and read back to verify.
- On boot, a latched emergency with a valid stored set restores those stations as blockers before the P4 accepts any clear.

## Rules
- A pressed station blocks Director and USB clear. It keeps blocking when offline, after a restart, after a timeout, or after a P4 reboot.
- A station releases only on a **fresh, valid report with `input_open=0` from the same ID with a matching stored MAC**. A stored MAC that is missing or different does not release, and it does not create a block either.
- Restart, reconnection and time alone never release.
- Unknown or corrupt stored data while latched **fails closed**: clear is blocked. Nothing is written over an unknown record.
- A failed or unverified write blocks clear until a verified write succeeds. Retries run at most every 5 s.
- A clear is refused until the station table has been restored and applied at boot.

## Clear reply codes
- `EMERGENCY:CLEAR:REJECTED:WIRELESS_ASSERTED` (any blocker, unknown boot state, unverified blocker write, or clear before boot restore completes).
- `EMERGENCY:CLEAR:REJECTED:PERSIST_FAILED` (latch CLEAR write unverified).

## Known limitations (not fixed here)
1. **Replacing a station changes its MAC.** A replacement with the same ID but new hardware cannot release the old record. Clear stays blocked until the service procedure below. This is intentional. Matching on ID alone would let any device claim an ID.
2. **Unknown boot state has no software exit.** Once the stored record is absent or corrupt while latched, clear is blocked until a service action. No code path clears it.
3. **Director display:** restored blockers are not shown as devices until a station re-announces, because the device list hides unseen stations.
4. **Write failure followed by power loss:** if a blocker write fails and power is lost before a later verified write, the restored set can miss a station. The window is the retry interval. This is the storage limit noted in the firmware document.
5. **Assert-only nodes** still cannot clear or prove release. Only a fresh report can.

## Supervised service recovery (documented procedure)
Use only when clear is blocked by an unknown record, or by a station that cannot be brought online.

1. **Keep outputs safe.** Do not restore power to the show. Leave Emergency latched. Keep all show outputs in their emergency-safe state.
2. **Physically inspect every Emergency Node.** Confirm, by hand, the state of each button and the ID printed on each unit. Record the results in the event log.
3. **Power each station and let it report.** If a station reports released with a matching MAC, normal clear works. Use the Director clear with the button released.
4. **For a permanently failed or replaced station**, the software has no supported path yet. Escalate to the designer (Toby) before any change. Do not clear the emergency by erasing storage on site without that review.
5. **Record** which station, ID and MAC, the inspection outcome, and who authorised the action.

## Proposed future implementation (not built)
A USB-only maintenance command, `ESTOP:BLOCKERS:RESET:<confirm>`. It would:
- refuse unless GPIO25 is released **and** the operator has confirmed a physical inspection of every station;
- clear only the stored blocker record, never the latch;
- write an audit line and reply with the record it removed;
- be absent from the Director and all wireless paths.
It needs review against the "no casual bypass, no automatic timeout, no undocumented remote clear" rule before it is built.

## MISSION 001E review

### Can the procedure be completed with the current firmware?
**No.** Step 4 (failed or replaced station) has no supported software path:
- A replacement station has a new MAC, so it cannot release the stored record.
- Clearing a stored record needs a firmware action that does not exist.
- An unknown boot state (corrupt or unverified storage) is blocked with no software exit.

This is an **unresolved limitation**. Do not treat the station-replacement or unknown-state cases as recoverable on site until a working, tested mechanism exists.

### Controlled physical-maintenance recovery (requirements for a future build)
Any recovery mechanism must meet all of these:
1. **Physical access only.** A USB maintenance session with the P4 in the installation, not a Director button and not any wireless or network path.
2. **Outputs safe first.** Emergency latched, all show outputs in their emergency-safe state, and the show unable to run.
3. **Explicit operator authority.** A named operator confirms, in writing and in the event log, that every Emergency Node was physically inspected, with its ID and button state recorded.
4. **GPIO25 released** and every station with a fresh released report, **or** the station physically removed from the installation and recorded as removed.
5. **Scoped action.** Removes only the stored blocker record. It never edits the latch byte, so the latch stays ACTIVE until a normal, verified CLEAR.
6. **Verification.** Read back the stored record, reboot, and confirm the P4 reports the record as cleared before any clear is accepted.
7. **Audit.** Every use writes a log line with the time, the operator, and the removed identities.

### Wireless trust boundary (Priority 4): unresolved
- ESP-NOW peers are added with `encrypt = false` (`firmware/c3-emergency-node/.../EspNowEmergencyTransport.cpp`).
- Announce and assert messages carry no signature, nonce or counter that the P4 checks. The announce `sequence` field is not checked for freshness or replay.
- A station's MAC address appears in clear text in every message. **A matching MAC is not authentication.**
- Consequence: a device within radio range can send a forged released announce that matches a stored ID and MAC. That would drop a stored blocker. The emergency itself stays latched, and the operator still has to clear through the Director with GPIO25 released. Still, this weakens the release guarantee, and it is a **High** item.
- Not fixed here. Dependency for the **Comms security mission**: authenticated, replay-protected station messages, for example a per-station key provisioned at commissioning with monotonic counters.
