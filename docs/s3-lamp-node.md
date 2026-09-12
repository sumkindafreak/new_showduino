# Showduino S3 Lamp Node — carbide lamp

Product: Showduino **1.0.0-rc.1**. Lamp firmware **0.3.0**. Protocol **1.0**. SHDO package **v2**.

The production Lamp Node is an **ESP32-S3 interactive carbide-lamp simulator**. It is not a Pixel Node and not a second Audio Node.

Do **not** flash this node until the GPIO checklist at the end of this document is completed.

## Architecture

```text
Director / Nodes
        ↓ ESP-NOW
S3 Communications Controller
        ↓ UART
P4 Show Engine          ← authoritative show / emergency
        ↓ ROUTE:LAMP:
S3 Lamp Node            ← local carbide machine, jewel, blow, Fermion FX
```

Logical identity is `LAMP-01` / `LAMP-02` (NVS). MAC is not the production identity. Friendly name is presentation metadata.

## Carbide machine

`OFF → STRIKING → IGNITING → BURNING`, plus `LOW_FLAME`, `UNSTABLE`, `FLARE`, `EXTINGUISHING`.

Physical striker and P4 `IGNITE` enter the **same** machine. Failed strikes exist only as a configurable percentage and default to **0**.

## Sensors

- **Mic:** non-blocking blow detector (baseline, relative rise, puff vs sustained). Not `analogRead > threshold`.
- **Light:** telemetry only. No automatic flame compensation in this pass.
- **Voltage:** raw ADC always; millivolts only when a calibration scale is stored. Never invent 5.00 V.

## Local audio

DFRobot Fermion DFPlayer Pro (DFR0768) over UART. Semantic roles: `STRIKE`, `IGNITION`, `BURN_LOOP`, `FLARE`, `EXTINGUISH`. Missing hardware must not block the lamp. This is **not** the Showduino Audio Node.

## Emergency (existing product policy — unchanged)

Emergency forces the jewel **bright white**. Clear returns to idle and does **not** resume the previous flame. The node cannot locally clear a system emergency.

This conflicts with carbide-flame realism. It is the current Showduino lamp/pixel emergency rule. Do not change it without an explicit product decision.

## Comms-loss (existing product policy — unchanged)

- Show-controlled + GRANT/keepalive stale (5 s): extinguish, report `COMMS_TIMEOUT`.
- Standalone local burn: flame may continue. Do not inherit Pixel-node blackout for interactive use.

## Commissioning WebUI

SoftAP `Showduino-Lamp-XXXX` on the ESP-NOW channel, typically `192.168.5.1`, password `showduino`. Identity, carbide tests, blow diagnostics, sensors, Fermion test, and unconfirmed GPIOs. Cannot clear emergency.

## Future Director / Studio

Director already shows lamp online/FX via P4 `LampNodeLink`. Future: show `LAMP-01` + carbide state (`BURNING`) and faults (`LOW VOLTAGE`, `LOCAL AUDIO FAULT`, `COMMS LOST`). Do not turn Director into a second lamp admin desk.

Studio should author `LAMP-01 IGNITE` / `UNSTABLE_FLAME` / `EXTINGUISH`. SHDO v2 already compiles those tokens to `LAMP:NODE:` commands. No v3 schema.

## GPIO checklist

See the physical GPIO checklist in the rebuild report and `BoardConfig.h`. Every lamp pin is `-1` until traced.
