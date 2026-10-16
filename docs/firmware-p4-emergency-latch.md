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

## MISSION 001G — actuation order, identity pinning, CI

### Actuation order (all assertion sources)
Local button, USB, Director command, and wireless assert now run in this order:
1. **RAM latch and every emergency-safe output.** Latch, show stop, pixels to white, audio, node commands, Director notice. Never waits on storage. Implemented in `emergencyEngageRamCb` and sequenced by `protocol/showduino_emergency_engage.h`.
2. **Blocker record** (`emergencyNodeLinkPersistBlockers`). Written and read back.
3. **Latch byte** (`emergencyLatchSetActive`), told whether the blocker record verified.
4. **Wireless only:** ACK to the asserting station, then the station table update.

A storage failure or delay after step 1 never undoes or delays step 1. The latch stays active in RAM and is retried every 5 s.

**Trade-off, stated plainly:** an assertion lost before step 2 or 3 is durable cannot survive a P4 reboot. Software cannot recover it. The physical button or a station re-assert must recreate it. Ordering only changes what happens while the powered P4 is running; it does not make a power cut safe.

**Not measured:** no timing was measured on hardware. The claim that outputs no longer wait on flash is from the code path, not from a bench measurement. Serial logging still runs before step 1 (`Serial.printf`). If the USB host stops reading, that can block. This is a bench item.

### Station identity pinning
A station's pinned ID and MAC are the record. An announce whose ID or MAC conflicts with the pinned record is **rejected whole**. It changes nothing: no liveness, no ID or MAC change, no release. The release rule reads the MAC pinned before the report.

A MAC match is continuity, not authentication. ESP-NOW frames are unsigned, so any radio in range can send a matching identity. The unauthenticated release problem remains a separate **High** release blocker for the Comms security mission.

### CI (GitHub Actions)
- The earlier "success" was false. GitHub's default Linux shell has no `pipefail`, so `arduino-cli ... | tee` hid compile failures. All jobs now use `shell: bash` (`-eo pipefail`).
- Diagnostics are visible as check-run annotations: GCC problem matcher, error and warning counts, and sketch size.
- New Stage Engine files include protocol headers by bare name. The workflow passes an absolute `-I` for the protocol directory.

### Tests
- Latch suite: **114 checks, all pass.** T33a–f: RAM first at t=0 under a 60 s storage delay, storage failure leaves outputs engaged, unverified blocker passed to latch, no second engage. T34–T41: forged and repeated announces rejected, pin survives, original station releases afterwards.
- Mutation checks: ordering reversed, or outputs gated on storage, caught. Identity check removed (the original defect) caught by 4 checks.
- Host suites: button suite pass; director sheet suite pass (pre-existing `snprintf` warning).

### Compile result (CI, this branch)
- Current: **0 errors, 1 warning** (ESP32 core `esp32-hal-spi.c:584`, not project code). Sketch **1182942 bytes (90%)**. Globals **81636 bytes (24%)**.
- Baseline `a8b3cb8`: 0 errors, 1 warning (same core warning). Sketch 1164198 bytes (88%). Globals 79964 bytes (24%).

**Not hardware-tested.** Compile success does not show the P4 behaves correctly.

## MISSION 001I — station freshness, persisted set, output order

### Station freshness (Requirement 1)
- A bare `ESTOP:ASSERT` never sets a station online and never refreshes its liveness. The shared model (`protocol/showduino_station_model.h`) and the P4 link (`applyStationLatch`) both follow this.
- Only a fresh, identity-validated full `ANNOUNCE` (or a `STATUS` from the single online station) restores ONLINE. Only a verified `ANNOUNCE` can record RELEASED.
- The global latch is still asserted immediately by any ASSERT.
- Tested end to end (S13): release confirmed → disconnect → bare ASSERT → clear refused → fresh released ANNOUNCE → clear accepted.

### Persisted blocker set (Requirement 2)
- Previously the stored set held only physically pressed stations. A station released and then disconnected dropped out of the record on the next write, so a reboot could lose a clear condition.
- A station is now stored while it is **asserted in the current episode**, or while it blocks clear. The clear gate and the stored set use the same shared rule (`showduino_station_model_required`).
- An accepted Director clear ends the episode (`emergencyNodeLinkEndEpisode`), then stores the set. A completed clear stores no stale blockers.
- Stored blockers come back as asserted, unseen, offline records. They need a fresh released ANNOUNCE before clear.
- The set is built by `showduino_station_model_collect_required`. If it cannot be built faithfully (more entries than the record holds, or a required station without an identity), that is a storage fault: clear stays blocked. Nothing is dropped silently.
- Storage is written after every accepted station line, and only when the stored set changes. A failed write marks the record dirty. Clear stays blocked until a write is verified.
- An unreadable record after a latched restart keeps the existing fail-closed unknown-boot state.
- Tests S14–S18 cover reboot after release-then-disconnect, a missing announcement after reboot, several stations in mixed states, a failed write with all stations released, storage recovery, an unreadable record, and the rule that a completed clear leaves no stored blocker.

### Output order (Requirement 3)
The functions were read before editing. The change is limited to `emergencyEngageRamCb` and `triggerEmergency` in `ShowduinoStageEngineP4.ino`.
- Now first, after the RAM state is set: `showduinoIOOnEmergency(true)`, `emergencyPixelsSetWhite()`, `showPixelsOnEmergency(true)`, `statusLedWrite(HIGH)`.
- Then: Serial and SD activation logs, show stop, ambience, node and remote propagation, the runtime stop, the plugin bus, Director notices, emergency audio, the `ACTIVATE` log record.
- The trigger banners, the superseded-clear notice and the `PERSIST_FAILED` record now follow the engage call. The blocker and latch writes still run after the outputs, as in 001F.
- The clear path is unchanged. The latch clear is still stored before RAM is released.
- "Deactivation" in the mission brief was read as the emergency output state change. The clear path was not reordered.

**Residual blocking, not fixed:** `emergencyPixelsSetWhite`, `showPixelsOnEmergency` and `showduinoIOOnEmergency` print to Serial internally. Those prints still sit inside the output path. A stalled USB host could delay them. This is a bench item.

### Not verified
- **No timing has been measured.** The ordering claim comes from the code path only. The P4 wrapper is compiled by CI and not exercised on the host.
- The C3 Arduino wrapper is not compiled by any CI job. Its protocol logic is covered by the integration suite.

### Remaining limitations
- **ESP-NOW is unauthenticated.** Release messages can be forged by any radio that speaks the protocol. This is a **High** release blocker for the Comms security mission. Identity (ID and MAC) matching is continuity, not authentication.
- **The authorised maintenance-recovery procedure is not implemented.** An unknown-boot record blocks clear until a service procedure resets it. The procedure is described in `docs/emergency-station-service-recovery.md` but is not in firmware.
- A late ASSERT after clear re-latches the P4. This is deliberate and conservative.
- A pressed state can lag one announce interval (2 s) when the station is already latched. Not measured.

### Hardware acceptance tests (not run)
1. Bench, outputs disconnected: press, release, confirm, clear through the Director. The legitimate sequence must succeed.
2. Station powered off while released: clear refused. Power back on, released ANNOUNCE: clear accepted.
3. P4 power-cycled with a stored blocker and the station offline: clear refused until the station announces.
4. SD or NVS write failure and unreadable record: clear refused, recovery works.
5. Scope: debounced input edge to first output pin change. Record the measured value. Do not claim it without the measurement.
6. Serial stopped being read by the host: outputs still change.
7. C3 build with its documented board configuration. No CI target exists yet.

### Host tests (this commit)
- Latch suite: 114 checks, all pass.
- Button suite: 43 checks, all pass.
- Director sheet suite: 12 checks, all pass.
- Station integration suite: 66 checks, all pass, compiled with `-Wall -Wextra -Werror`.
- Mutation checks on the new rules: bare ASSERT sets online (caught, 4 fails); required set = pressed only (the 001H rule, caught, 6 fails); episode never ends (caught, 3 fails); silent drop of an unbuildable set (caught, 3 fails).

### Compile result (CI)
CI run 38949071891 on commit `1689919` (GitHub Actions, real compile):
- Current: **0 errors, 1 warning** (ESP32 core `esp32-hal-spi.c:584`, not project code). Sketch **1184792 bytes (90%)**. Globals **81636 bytes (24%)**.
- Baseline `a8b3cb8`: 0 errors, 1 warning (same core warning). Sketch 1164198 bytes (88%). Globals 79964 bytes (24%).
- Host-tests job, same run: all four steps passed (latch, button, director sheet, station integration).

Sketch growth from 001G (1182942 to 1184792 bytes, +1850) reflects the station-freshness and episode code. Globals are unchanged.

**Not hardware-tested.** Compile success does not show the P4 behaves correctly.
