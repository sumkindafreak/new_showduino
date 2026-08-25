# Showduino Onboard C6 Communications Engine

```text
Status: TARGET / BRING-UP
Hardware: ESP32-C6 integrated in Waveshare ESP32-P4-Module-DEV-KIT
Role: Showduino Communications Engine
```

The onboard ESP32-C6 is now the **selected final Communications Engine hardware** for the Stage Controller. This removes the need for a separate external C3/SUE board.

## Important: target hardware does not mean this sketch is production-ready

The existing sketch in this folder was an early ESP-NOW bridge prototype. It contains placeholder assumptions about a C6↔P4 UART on C6 GPIO4/5.

Those values are **not the final product transport contract**.

The Waveshare ESP32-P4 module integrates the C6 with the P4 using **SDIO** for Wi-Fi/Bluetooth expansion. The module separately exposes C6 UART pins for flashing/debugging.

Do not wire or document the placeholder GPIO4/5 UART as if it were the canonical internal P4/C6 link.

## Target topology

```text
Director ESP32-S3
    → ESP-NOW
this onboard ESP32-C6
    → integrated Stage Controller transport
ESP32-P4 Show Engine
```

```text
Showduino Node
    → ESP-NOW
this onboard ESP32-C6
    → ESP32-P4 Show Engine
```

```text
Browser
    → Wi-Fi
this onboard ESP32-C6
    → P4 Web services
```

## Required migration parity

Before this firmware is promoted to `ACTIVE`, it must prove:

1. Director commands reach the P4.
2. P4 ACK/state replies reach the Director.
3. Node commands and node replies route correctly.
4. Emergency activate/clear traffic is reliable.
5. Wi-Fi AP/STA provides the required Studio/WebUI front door.
6. Heartbeat/link recovery works.
7. Unsupported routes never report false success.
8. The C6 can be recovered/reflashed repeatably.

The previous external C3 implementation at `firmware/c3-supermini-espnow-bridge/` remains the compatibility/reference implementation until all of the above pass.

## C6 flashing / recovery warning

Waveshare ships the ESP32-C6 with factory firmware. **Identify and preserve a recovery path before replacing it.**

Waveshare's documented C6 flashing process:

```text
1. Hold/pull C6_IO9 LOW during power-on/reset to enter C6 download mode.
2. Put the P4 into download mode as required by the board procedure.
3. Flash the C6 through C6_U0RXD / C6_U0TXD.
```

`C6_IO9` is a **C6** GPIO. It is not P4 GPIO9; P4 GPIO9 belongs to the onboard ES8311 audio path.

## Constitution

> The Communications Engine transports.

It must not:

- Run the show timeline.
- Own authoritative show state.
- Make show-level decisions.
- Control local show outputs as an authority.
- Return success for work that never happened.

## Current sketch

```text
firmware/p4-c6-espnow-bridge/ShowduinoP4C6EspNowBridge/
```

Treat it as source material / bring-up code until the internal transport architecture is implemented deliberately.

## References

- Hardware baseline: [`docs/hardware-baseline-2026-08-25.md`](../../docs/hardware-baseline-2026-08-25.md)
- Architecture: [`docs/architecture.md`](../../docs/architecture.md)
- Repository status: [`docs/repository-status.md`](../../docs/repository-status.md)
- Waveshare board docs: https://docs.waveshare.com/ESP32-P4-Module-DEV-KIT
- Waveshare C6 flashing FAQ: https://docs.waveshare.com/ESP32-P4-Module-DEV-KIT/FAQ
