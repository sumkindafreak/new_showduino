# Showduino Onboard C6 Communications Engine

```text
Status: TARGET / BRING-UP
Hardware: ESP32-C6 integrated in Waveshare ESP32-P4-Module-DEV-KIT
Role: Showduino Communications Engine
```

The onboard ESP32-C6 is the selected final Communications Engine hardware for the Stage Controller. This removes the need for a separate external C3/SUE board once feature parity is proven.

## First qualification: preserve the factory C6 firmware

Before replacing anything on the C6, run:

```text
firmware/p4-c6-espnow-bridge/hosted-link-qualification/
```

That ESP-IDF project runs on the **P4**, leaves the C6 factory firmware untouched, brings up the internal SDIO transport, reads the C6 Wi-Fi STA MAC and performs a real Wi-Fi scan.

A PASS proves the physical/internal path:

```text
P4 -> SDIO -> onboard C6 -> Wi-Fi radio
```

See the qualification project's README for build/flash instructions.

## Confirmed internal P4/C6 wiring

The Waveshare module follows the standard P4/C6 SDIO layout:

| Function | P4 GPIO |
|---|---:|
| SDIO D0 | 14 |
| SDIO D1 | 15 |
| SDIO D2 | 16 |
| SDIO D3 | 17 |
| SDIO CLK | 18 |
| SDIO CMD | 19 |
| C6 CHIP_PU / reset | 54 |

These pins are board resources for the onboard C6 and are reserved whenever the C6 SDIO transport is active.

## Critical migration conflict

The current Arduino Stage Controller sketch still uses **P4 GPIO18 RX / GPIO17 TX** as the old external-C3 UART compatibility link.

That cannot coexist with the onboard C6 SDIO transport because those same pins are physically:

```text
GPIO17 = C6 SDIO D3
GPIO18 = C6 SDIO CLK
```

Therefore:

- the existing external-C3 Stage firmware remains a rollback build only;
- the hosted-link qualification is a separate P4 image;
- production C6 migration must remove/feature-gate the GPIO17/18 UART before enabling SDIO in the main Show Engine firmware.

Do not try to run both transports at once.

## ESP-NOW limitation in stock ESP-Hosted

The factory C6 image is useful for Wi-Fi/Bluetooth hosted operation, but Espressif's normal ESP-Hosted host API does **not currently expose ESP-NOW** to the P4 application.

Showduino therefore needs a deliberate C6-side extension/custom service for its Director/node ESP-NOW packets while preserving the internal SDIO transport.

The plan is:

```text
Director / Nodes
    -> ESP-NOW
custom Showduino service on onboard C6
    -> internal SDIO / hosted transport
P4 Show Engine
```

and the reverse path for ACK/state/replies.

We will not replace the factory C6 firmware until the factory hosted link has been bench-qualified and the recovery procedure is understood.

## Existing prototype sketch

```text
firmware/p4-c6-espnow-bridge/ShowduinoP4C6EspNowBridge/
```

This was an early ESP-NOW bridge prototype. It contains placeholder assumptions about a C6↔P4 UART on C6 GPIO4/5.

Those values are **not the final product transport contract** and must not be wired/documented as the internal P4/C6 link.

## Target topology

```text
Director ESP32-S3
    -> ESP-NOW
onboard ESP32-C6
    -> internal Stage Controller transport
ESP32-P4 Show Engine
```

```text
Showduino Node
    -> ESP-NOW
onboard ESP32-C6
    -> ESP32-P4 Show Engine
```

```text
Browser
    -> Wi-Fi
onboard ESP32-C6
    -> P4 Web services
```

## Required migration parity

Before the onboard C6 path is promoted to `ACTIVE`, it must prove:

1. Factory P4↔C6 SDIO/radio qualification passes.
2. Director commands reach the P4.
3. P4 ACK/state replies reach the Director.
4. Node commands and node replies route correctly.
5. Emergency activate/clear traffic is reliable.
6. Wi-Fi AP/STA provides the required Studio/WebUI front door.
7. Heartbeat/link recovery works.
8. Unsupported routes never report false success.
9. The C6 can be recovered/reflashed repeatably.

The previous external C3 implementation at `firmware/c3-supermini-espnow-bridge/` remains the compatibility/reference implementation until all of the above pass.

## C6 flashing / recovery warning

Waveshare ships the ESP32-C6 with factory firmware. **Identify and preserve a recovery path before replacing it.**

Waveshare's documented C6 flashing process uses the module's C6 boot/programming signals:

```text
C6_IO9
C6_U0RXD
C6_U0TXD
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

## References

- Hardware baseline: [`docs/hardware-baseline-2026-08-25.md`](../../docs/hardware-baseline-2026-08-25.md)
- Hardware/resource map: [`docs/hardware-pinout.md`](../../docs/hardware-pinout.md)
- Architecture: [`docs/architecture.md`](../../docs/architecture.md)
- Repository status: [`docs/repository-status.md`](../../docs/repository-status.md)
- Qualification project: [`hosted-link-qualification/`](hosted-link-qualification/)
