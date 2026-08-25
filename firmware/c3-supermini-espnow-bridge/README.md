# Showduino Communications Engine — External ESP32-C3 SuperMini

```text
Status: COMPATIBILITY / ROLLBACK
Role: previous Showduino Communications Engine implementation
Final Stage Controller hardware: NOT REQUIRED
```

This firmware is the previously working external C3 communications path.

It remains in the repository because it is valuable migration and rollback code while the Stage Controller's **onboard ESP32-C6** is brought to feature parity.

It is no longer the canonical physical communications board for new Stage Controller builds.

## Previous working topology

```text
Director  --ESP-NOW-->  external C3  --UART-->  P4 Show Engine
Node      --ESP-NOW-->  external C3  --UART-->  P4 Show Engine
Browser   --Wi-Fi---->  external C3  --UART-->  P4 Web services
```

## Final target topology

```text
Director / Nodes / Browser
        → onboard ESP32-C6
        → integrated Stage Controller transport
        → ESP32-P4 Show Engine
```

Target C6 tree:

```text
firmware/p4-c6-espnow-bridge/
```

## Constitution

> The Communications Engine transports.

This compatibility firmware must not:

- Run the show timeline
- Own authoritative show state
- Make show-level decisions
- Return success for unimplemented pixel/audio actions

## What this compatibility firmware contains

- Director ESP-NOW ↔ P4 UART transport
- Relay-node ESP-NOW routing
- Node replies back to P4
- Wi-Fi AP front door for Studio
- Web/API tunnel behaviour
- Link/heartbeat logic
- DS3231-based time service from the previous hardware generation

The DS3231 code is retained only because this tree represents the last working external-C3 stack. The final Stage Controller time source is the ESP32-P4 RTC/system time.

## Do not extend this as the final product

New Stage Controller work should go toward:

- onboard C6 transport parity
- P4 RTC time service
- P4-hosted Web services
- preserving the same authority/transport separation

Fix this compatibility tree only when needed to understand, test or recover the migration.

## Archive gate

Do not archive/delete this folder until the onboard C6 proves:

```text
Director commands + replies
node routing + replies
emergency traffic
WebUI/Wi-Fi transport
link recovery
```

See [`docs/repository-status.md`](../../docs/repository-status.md), [`docs/architecture.md`](../../docs/architecture.md), and [`docs/hardware-baseline-2026-08-25.md`](../../docs/hardware-baseline-2026-08-25.md).
