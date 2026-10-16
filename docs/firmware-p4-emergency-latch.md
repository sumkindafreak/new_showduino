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
