# Showduino Migration Plan

**Status: LEGACY / SUPERSEDED.** This document records the 25 August 2026 onboard-C6 migration proposal; it is not the current implementation plan and must not be executed as the product baseline.

The supported current path is Director ESP32-S3 → ESP-NOW → dedicated ESP32-S3 Communications Engine → UART → ESP32-P4 Show Engine. The onboard C6 is unused/reserved. See [`final-hardware-architecture.md`](final-hardware-architecture.md).

## Goal

Prove one complete, safe Showduino scenario on the final lean Stage Controller hardware without adding unnecessary modules.

Historical proposed baseline:

```text
Waveshare ESP32-P4-Module-DEV-KIT
├── ESP32-P4 Show Engine
├── onboard ESP32-C6 Communications Engine
├── P4 RTC / rechargeable backup connection
├── onboard ES8311 system audio
├── onboard microSD / Ethernet / USB
├── emergency GPIO24/25
└── external PCM5102A show audio
```

## Rule one — preserve working fallbacks during migration

The external C3 and DS3231 are removed from the **physical target**, but their working firmware remains in the repository until the onboard replacements are proven.

Do not delete rollback code before parity testing.

## Phase 1 — freeze the hardware baseline

Status: **DECIDED**

- P4 remains the Show Engine.
- This superseded proposal would have replaced separate C3/SUE hardware with the onboard C6.
- P4 RTC replaces external DS3231 hardware.
- Onboard ES8311 handles Showduino/system sounds.
- PCM5102A remains for show/programme audio.
- GPIO24 remains emergency NeoPixel.
- GPIO25 remains momentary emergency trigger.
- microSD, Ethernet and USB remain native Stage Controller resources.

Reference: `docs/hardware-baseline-2026-08-25.md`.

## Phase 2 — qualify onboard C6 programming/recovery

Before replacing factory C6 firmware:

1. Record the board/module revision.
2. Record/recover the factory C6 firmware source/version where possible.
3. Verify the C6 UART flashing connection.
4. Verify `C6_IO9` download-mode procedure.
5. Prove the C6 can be reflashed/recovered without losing access to the P4.

Waveshare documents C6 download mode as `C6_IO9` LOW during power/reset, with flashing over `C6_U0RXD` / `C6_U0TXD`.

## Historical Phase 3 — move Communications Engine to onboard C6 (superseded; do not execute)

Target tree:

```text
firmware/p4-c6-espnow-bridge/
```

Required parity with the previous external C3 bridge:

1. Director ESP-NOW receive.
2. P4 command delivery.
3. P4 ACK/state replies back to Director.
4. Relay/node forwarding.
5. Node replies back to P4.
6. Emergency activate/clear transport.
7. Link heartbeat/recovery.
8. Wi-Fi AP/STA front door for Studio/WebUI.
9. No false success for unsupported routes.

The board's integrated C6/P4 relationship uses SDIO for wireless expansion. The current experimental C6 sketch's placeholder UART assumptions are not the final product transport contract.

**Exit criterion:** the Director, WebUI and one real node work through the onboard C6 with the external C3 physically absent.

## Phase 4 — move time authority to P4 RTC

Remove the DS3231 dependency from the product path.

Tasks:

1. Create P4 time service owned by the Show Engine.
2. Publish time to Director/WebUI.
3. Define boot-time clock-set policy.
4. Integrate rechargeable RTC backup support where the framework permits.
5. Test time retention through the intended power/sleep sequence.
6. Remove DS3231-specific UI labels from the production path.

**Exit criterion:** Showduino boots with useful time, publishes it consistently and passes the chosen backup-power test without a DS3231 connected.

## Phase 5 — onboard Showduino/system audio

Use the Waveshare ES8311 + NS4150B path.

Initial system sounds:

```text
boot
ready
loaded
armed
warning
error
emergency acknowledgement
restart/shutdown
```

Tasks:

1. Initialise I2C on GPIO7/8.
2. Initialise ES8311 I2S on GPIO9-13.
3. Control amplifier enable on GPIO53.
4. Play one small WAV from P4 SD.
5. Make missing sound assets non-fatal.
6. Add simple system-volume control.

**Exit criterion:** boot and ready sounds play from the onboard speaker without an extra audio controller.

## Phase 6 — restore/qualify PCM5102A show audio

Current show-audio pins:

```text
WS    GPIO20
BCLK  GPIO21
DOUT  GPIO22
```

Tasks:

1. Play a show WAV from P4 SD.
2. Stop/pause deterministically.
3. Report play state/faults.
4. Connect show audio to timeline cues.
5. Verify emergency stops/replaces show audio according to safety policy.

## Phase 7 — I2S arbitration

The ESP32-P4 has one I2S peripheral, while the baseline has two physical audio output roles.

Implement explicit ownership:

```text
IDLE
SYSTEM_AUDIO
SHOW_AUDIO
FAULT
```

Rules:

- Active show audio has priority over routine UI sounds.
- Routine system sounds cannot corrupt show audio.
- Emergency logic can deliberately seize/stop audio according to policy.
- Do not claim simultaneous independent streams until proven.

## Phase 8 — one complete scenario

Minimum proof:

1. Stage Controller boots.
2. Onboard system boot/ready sound plays.
3. Director connects through onboard C6.
4. Show package loads from P4 SD.
5. Show starts from Director.
6. PCM show audio plays.
7. Timeline drives at least one pixel/output action.
8. Emergency button GPIO25 latches emergency.
9. GPIO24 emergency pixels activate.
10. Show audio stops/safes correctly.
11. Clear is rejected while GPIO25 remains held.
12. After release and explicit clear, runtime returns safe/idle and does not auto-resume.

**This is the primary milestone.**

## Phase 9 — tidy compatibility code only after proof

After the complete scenario passes:

- Mark external C3 bridge legacy/archive candidate.
- Archive DS3231-only compatibility services if no longer needed.
- Remove obsolete documentation paths.
- Rename Stage Engine folder terminology when convenient.
- Keep diagnostic sketches that still save bring-up time.

## Success definition

Showduino's current hardware migration succeeds when the Stage Controller can run the complete proof above using:

```text
Director + P4 board + onboard C6 + P4 RTC + onboard system audio + PCM show audio
```

with no separate C3/SUE or DS3231 module required inside the Stage Controller.
