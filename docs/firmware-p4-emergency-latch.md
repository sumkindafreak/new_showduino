# P4 Stage Engine — persistent emergency latch

Status: **software implemented on branch `fix/emergency-persistent-latch`. Not hardware-tested.**
Scope: ESP32-P4 Stage Engine (`firmware/stage-engine-p4/ShowduinoStageEngineP4`).
This is an entertainment emergency function. It is not a certified emergency-stop or life-safety system.

## Behaviour

- **Assertion** (GPIO25 debounced open loop, Wireless, USB `EMERGENCY:STOP`, Director): the latch is written to NVS (`showduino` / `estop_latch`) **before** the RAM latch engages. A verified write is required for the stored state to match. A failed write is logged and retried. It never prevents the emergency from engaging.
- **Reboot**: `emergencyLatchRestoreAtBoot()` runs straight after the emergency input is initialised, before pixels, SD, show runtime, audio or node links are touched. A stored ACTIVE restores the latch. Outputs engage once the runtime is idle (`engageRestoredEmergencyAtBoot()`). Playback is never resumed.
- **Physical button released before restart**: the latch still restores. Release never clears.
- **Physical button held at restart**: the restored latch holds. The button is re-read by the normal debounced input service and cannot clear the latch.
- **Clear**: only through the existing supported paths. Director request then confirm, with the button released (`emergencyInputConfirmDirectorClear`), or the USB maintenance path, which also requires the button released. Clear persists CLEAR **first**. If the CLEAR write cannot be verified, the RAM latch stays active and the reply is `EMERGENCY:CLEAR:REJECTED:PERSIST_FAILED`.
- **Show playback**: `SHOW_STATE_EMERGENCY_STOP` at boot. Existing run/load/resume guards reject with `REJECTED:SHOW:EMERGENCY_ACTIVE` until a clear.
- **Storage read failure, open failure or corrupt byte at boot**: fail-safe. The latch is restored (LATCHED). The unit does not return to normal playback. Only a verified manual clear releases it.
- **Storage write failure while asserted**: the RAM latch stays active. Retries occur at most every 5 s while the write is unverified, and only then.

## Protocol and file compatibility

- No new commands. No new remote clear route.
- One **additive** reply code: `EMERGENCY:CLEAR:REJECTED:PERSIST_FAILED`. Director and USB clients that do not know it must treat any unknown rejection as "still latched". **Director handling of this code is not verified in this branch.**
- Show (`.shdo`) format unchanged.
- `EmergencyNodeLink` publishes no source label for restored latches (`gEmergencySourceId = 0`). This is a known gap. Downstream nodes still receive the latch state.

## Tests (host, policy only)

- `tools/emergency-latch-tests/run_tests.sh`: 24 checks plus a transition-only check (1000 assertions → 1 write). Covers the boot, reboot, held and released cases, invalid clear, valid clear, and open, read, write and read-back failures. Mutation check: inverting the fail-safe rule makes T8a–T8c fail.
- The NVS wrapper and sketch wiring are **not** exercised by host tests. A syntax check of `EmergencyLatchStore.cpp` against stub headers passed. Full firmware compilation **has not been run** (no Arduino toolchain in the build environment).

## Limitations

- Power removal during an NVS write can leave the namespace in a state the wrapper reads as FAULT. That is fail-safe (latched), but it needs hardware confirmation.
- NVS flash wear: writes happen only on latch transitions.
- Wireless emergency has no per-source release tracking. A clear checks the GPIO25 button only, as before.
- A Stage Engine that loses power cannot control its outputs. Outputs themselves may default to their own hardware states. Wiring and the power path are not verified here.
- Emergency audio depends on SD and the audio hardware. Audio failure keeps the latch active.

## Review findings (MISSION 001B)

**A. Restored propagation.** At boot the latch is restored, then outputs engage once the runtime is idle. The Audio Node and Lamp Node are resynced to P4 state once per second by `reconcileAudioNodeEmergency` / `reconcileLampNodeEmergency` (`EmergencyInput.cpp`). That covers nodes that connect late. Pixel, MOSFET and Emergency Nodes have **no** resync. Their emergency state is sent only on transitions, so a node that reconnects during an emergency does not receive it again. This is not fixed in this change. The fix is a matching once-per-second resync for each node, following the Audio/Lamp pattern. It needs hardware confirmation for each node.

**B. Wireless release tracking.** Confirmed defect before this change. `applyEmergencyClear()` and the clear gates checked only P4 GPIO25, so a Director clear could release while a wireless station was still pressed. A station repeats its assert while pressed, which limits the exposure, but a station that went silent could be left out of the picture. Fix: `showduino_station_clear_blocked()` (protocol header) refuses a Director or USB clear while any Emergency Node's most recent report has `input_open=1`. The refusal reply is `EMERGENCY:CLEAR:REJECTED:WIRELESS_ASSERTED`. A released station (`input_open=0`) never blocks, so the latch cannot deadlock. A station that was pressed and then went silent **still blocks**, because its state is unknown. Recovery is to power-cycle the station, which reports its real state on boot.

**C. Director handling of `PERSIST_FAILED` and `WIRELESS_ASSERTED`.** The Director (`firmware/director-esp32-8048s050`) matches the `EMERGENCY:CLEAR:REJECTED:` prefix. Unknown codes fall to the generic branch: it shows "Emergency clear rejected", hides the clear dialog and keeps the lock. The lock is released only by `STATUS:ECLEARED` or by a runtime mirror that leaves EMERGENCY_STOP, and neither is sent on a refusal. So a refused clear is never shown as a success. The operator message is generic. A specific message for each code is recommended as a follow-up.

**D. Storage limits.** NVS persistence cannot guarantee the latch across power loss in these cases:
- A failed ACTIVE write followed by power loss. The RAM latch is lost. The retry runs every 5 s, so the window is roughly one retry interval.
- Loss of power during an NVS write. The namespace may read as FAULT, which is fail-safe (latched), or the previous value may survive.
- Loss of P4 power generally. Outputs are not held by software once the P4 is unpowered. Their safe states depend on wiring and the power path, which are not verified here.

## MISSION 001C — node synchronisation and wireless release policy

### Node synchronisation (Priority 2)
- **Audio and Lamp**: unchanged. Once-per-second resync against P4 state (`EmergencyInput.cpp`).
- **Pixel and MOSFET**: new `reconcilePixelEmergency` / `reconcileMosfetEmergency`, run from their loops. Each node's announce `state` is compared with the P4 latch. A mismatch sends `EMERGENCY:STOP` (MOSFET also `MOSFET:ALL:OFF`) or `EMERGENCY:CLEAR`, at most once per second per node. Sending stops as soon as the node's announce reports the new state, so there are no repeated effect restarts.
- **MOSFET outputs**: the node turns all outputs OFF on `EMERGENCY:STOP`, and on comms or authority loss (`mosfet-node-esp32`). P4 also zeroes its cached levels. Identifier lighting on MOSFET nodes is documented as leaving outputs OFF (`MosfetIdentifierPixels.cpp`), and no path maps emergency lighting to an output.
- **Emergency (wireless) node**: **assert-only**. It never receives emergency-state commands as a "set state" and never clears P4. P4 sends it `ESTOP:ACK:LATCHED` if its announce says not acked, and `ESTOP:GLOBAL:OBSERVED` if it is still latched after a P4 clear. A station clears its own latch only on `GLOBAL:OBSERVED` with its button released. This is documented, not a new behaviour.
- A node that is still pressed will re-latch on `GLOBAL:OBSERVED`. That is correct.

### Wireless release policy (Priority 3)
- Each station's latest report wins: an assert sets input pressed, and an announce sets `input_open` from the node.
- **Disconnect while asserted**: the station record keeps `input_open=1`, so clear stays blocked. Offline does not clear input.
- **Restart alone**: nothing in the reconnect path resets `input_open`. A restart does not release the block.
- **Release**: only a fresh report with `input_open=0` releases.
- **Stale or contradictory reports**: a pressed station that went silent blocks clear. There is no timeout release. Recovery is to power-cycle that station, which reports its real input state.
- **Limitation, not fixed**: a P4 reboot drops the station table (`emergencyNodeLinkBegin`). Until stations re-announce, the restored latch has no station blocking state, and a clear could be accepted in that short window. Fix needs the assertion source persisted with the latch. This is not done here.

### Compatibility
- No change to `.shdo` or show formats.
- No change to existing wire commands. New traffic uses existing commands only: `EMERGENCY:STOP`, `EMERGENCY:CLEAR`, `MOSFET:ALL:OFF`, `ESTOP:ACK:LATCHED`, `ESTOP:GLOBAL:OBSERVED`.
- Added reply code: `EMERGENCY:CLEAR:REJECTED:WIRELESS_ASSERTED` (in addition to `PERSIST_FAILED`).

### Tests (host, policy only)
- Latch and policy suite: ALL PASS, including T11 (resync decisions), T12 (ack and global-clear decisions) and T13 (disconnect, restart, continued assertion, legitimate release, stale, contradictory).
- Mutation check: a mutant that ignores `input_open` when a station is offline fails 3 checks. A mutant that never sends `EMERGENCY:STOP` fails 2 checks. Both exit 1.
- Not compiled for the target. The node-link edits are unverified until the P4 firmware builds.

## MISSION 001D — persistent wireless blockers

- **Stored:** the set of stations whose last valid report is pressed (ID and MAC) in `estop_blk`. Written on change only, read back to verify.
- **Restored:** a stored set is staged at boot right after the latch is restored, and applied in `emergencyNodeLinkBegin`. Clear is refused before that point.
- **Release:** only a fresh released report with a matching stored MAC. See `docs/emergency-station-service-recovery.md`.
- **Fail closed:** missing, corrupt or unverified data blocks clear. A latched boot with an unknown record never overwrites it.
- **Limitations:** replacing a station changes its MAC, which blocks release until service. There is no software exit from an unknown boot state. Full list in the service document.
- **Tests:** latch suite T14–T23 (codec, reboot, reconnect, release, indefinite offline, multiple stations, write failure, corrupt and absent data, repeated announces). Mutation checks: four mutants, all caught.
- **Not compiled for the target. Not hardware-tested.**

## MISSION 001E — one coherent safety-state model

- **Latch byte** has three values: `0xA5` ACTIVE with a verified blocker record, `0xA6` ACTIVE with an **unverified** record, and `0x5A` CLEAR. Writes are transition-only and read back.
- **Assertion order:** the blocker record is written first, then the latch takes the verified or unverified value. If the blocker write fails, the latch still engages in RAM and is stored as unverified.
- **Boot:** a latch byte that is `0xA6`, unreadable or corrupt, or a latched boot with a missing or corrupt blocker record, is **unknown**. Clear is refused. Unknown storage is never overwritten.
- **Retry:** every 5 s, the blocker write is retried, then the latch is reconciled. The latch converges to `0xA5` once the record verifies.
- **Known gap:** if the latch write also fails, the assertion is held only in RAM. A restart before any verified write loses it, and the physical button (or a station re-assert) must re-create it. Software cannot recover an assertion that was never durably stored.
- **Tests:** latch suite T24–T32 (value mapping, transitions, failed writes, interrupted assertion, upgrade with a missing record, corrupt byte, crash between writes). Mutation checks: four mutants, all caught.
- **Not compiled for target. Not hardware-tested.**
