# Showduino Onboard C6 Communications Engine

```text
Status: TARGET / BRING-UP
Hardware: ESP32-C6 integrated in Waveshare ESP32-P4-Module-DEV-KIT
Role: Showduino Communications Engine
Primary development workflow: Arduino IDE
```

The onboard ESP32-C6 is the selected final Communications Engine hardware for the Stage Controller. This removes the need for a separate external C3/SUE board once feature parity is proven.

## First qualification — Arduino IDE

The normal Showduino qualification path is now:

```text
firmware/p4-c6-espnow-bridge/arduino-hosted-link-qualification/ShowduinoP4C6HostedQualification/
```

Open `ShowduinoP4C6HostedQualification.ino` in Arduino IDE and flash it to the **P4**.

The sketch leaves the C6 factory firmware untouched and proves:

```text
P4 -> internal SDIO -> onboard C6 -> Wi-Fi radio
```

It configures the C6 SDIO link with Arduino's `WiFi.setPins()`, starts the hosted Wi-Fi interface, reads the C6 STA MAC and runs a real Wi-Fi scan.

The neighbouring `hosted-link-qualification/` ESP-IDF project is retained only as a lower-level alternate/reference diagnostic. It is not required for the normal Showduino workflow.

## Arduino requirement

Use `esp32 by Espressif Systems` **3.3.x or newer**. Showduino's current bench core is 3.3.11.

Arduino-ESP32 3.3.x contains the P4 ESP-Hosted / Wi-Fi Remote support required by this board and exposes:

```cpp
WiFi.setPins(clk, cmd, d0, d1, d2, d3, rst);
```

## Confirmed internal P4/C6 wiring

| Function | P4 GPIO |
|---|---:|
| SDIO D0 | 14 |
| SDIO D1 | 15 |
| SDIO D2 | 16 |
| SDIO D3 | 17 |
| SDIO CLK | 18 |
| SDIO CMD | 19 |
| C6 CHIP_PU / reset | 54 |

These are board resources and are reserved whenever the onboard C6 SDIO transport is active.

## Critical migration conflict

The current production/rollback Arduino Stage sketch still uses:

```text
P4 RX GPIO18 <- external C3 TX
P4 TX GPIO17 -> external C3 RX
```

Those pins are also:

```text
GPIO17 = onboard C6 SDIO D3
GPIO18 = onboard C6 SDIO CLK
```

Therefore the old external-C3 UART and the onboard-C6 SDIO transport are mutually exclusive firmware modes. Do not enable both at once.

The external-C3 firmware remains a rollback/reference implementation until the onboard C6 path reaches feature parity.

## ESP-NOW limitation in stock ESP-Hosted

The factory C6 image handles hosted Wi-Fi/Bluetooth, but the standard hosted host API does not currently expose ESP-NOW directly to the P4 application.

Showduino therefore needs a deliberate C6-side ESP-NOW service/extension:

```text
Director / Nodes
    -> ESP-NOW
onboard C6 Showduino service
    -> internal P4/C6 transport
P4 Show Engine
```

and the reverse path for P4 ACK/state replies.

We will not replace the factory C6 firmware until the Arduino hosted-link qualification passes and the C6 recovery procedure is understood.

## Existing prototype sketch

```text
firmware/p4-c6-espnow-bridge/ShowduinoP4C6EspNowBridge/
```

This is early source material only. Its old C6 GPIO4/5 UART assumptions are not the final internal P4/C6 transport contract.

## Required migration parity

Before the onboard C6 path becomes `ACTIVE`, it must prove:

1. Arduino P4↔C6 hosted-link/radio qualification passes.
2. Director commands reach the P4.
3. P4 ACK/state replies reach the Director.
4. Node commands and replies route correctly.
5. Emergency activate/clear traffic is reliable.
6. Wi-Fi AP/STA provides the Studio/WebUI front door.
7. Heartbeat/link recovery works.
8. Unsupported routes never report false success.
9. C6 recovery/reflash is repeatable.

## C6 flashing / recovery warning

Waveshare ships the ESP32-C6 with factory firmware. Preserve that recovery path before replacing it.

Programming signals:

```text
C6_IO9
C6_U0RXD
C6_U0TXD
```

`C6_IO9` is a C6 GPIO, not P4 GPIO9. P4 GPIO9 belongs to the onboard ES8311 audio path.

## Constitution

> The Communications Engine transports.

It must not run the show timeline, own authoritative show state, make show-level decisions, or report success for work that never happened.

## References

- Arduino qualification: [`arduino-hosted-link-qualification/`](arduino-hosted-link-qualification/)
- Low-level qualification reference: [`hosted-link-qualification/`](hosted-link-qualification/)
- Hardware baseline: [`docs/hardware-baseline-2026-08-25.md`](../../docs/hardware-baseline-2026-08-25.md)
- Hardware/resource map: [`docs/hardware-pinout.md`](../../docs/hardware-pinout.md)
- Architecture: [`docs/architecture.md`](../../docs/architecture.md)
- Repository status: [`docs/repository-status.md`](../../docs/repository-status.md)
