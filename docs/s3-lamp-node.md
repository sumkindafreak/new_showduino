# Showduino S3 Lamp Node — carbide lamp

Product: Showduino **1.0.0-rc.1**. Lamp firmware **0.3.3**. Protocol **1.0**. SHDO package **v2**.

The production Lamp Node is an **ESP32-S3 interactive carbide-lamp simulator**. It is not a Pixel Node and not a second Audio Node.

The **same firmware** is a complete standalone carbide-lamp prop and a managed Showduino node. Do not flash a second image to change roles. Production GPIOs are confirmed in `BoardConfig.h`.

## Architecture

```text
Director / Nodes
        ↓ ESP-NOW
S3 Communications Controller
        ↓ UART
P4 Show Engine          ← authoritative show / emergency after GRANT
        ↓ ROUTE:LAMP:
S3 Lamp Node            ← local carbide machine, jewel, blow, Fermion FX
```

Logical identity is `LAMP-01` / `LAMP-02` (NVS). MAC is not the production identity. Friendly name is presentation metadata.

## Modes

| Product mode | Internal owner | Meaning |
|--------------|----------------|---------|
| STANDALONE | SEARCHING or STANDALONE | Local striker, blow, Fermion, and WebUI may control the lamp |
| SHOWDUINO | SHOW_CONTROLLED | P4 GRANT is live. WebUI is status / commissioning / diagnostics |
| EMERGENCY | EMERGENCY | Jewel bright white. Button, WebUI, standalone, audio, and flame are overridden |

Startup does **not** sit waiting for the P4. The physical lamp is live immediately. After ~8 s without GRANT the owner label becomes STANDALONE. Hearing ESP-NOW is not ownership.

If Showduino appears later, the existing GRANT / announce path transitions into SHOWDUINO mode without reboot. The current flame is kept. Stale P4 FX are **not** applied. The node waits for a fresh authoritative command.

If Showduino disappears, GRANT keepalive must go stale (~8 s). A momentary packet gap is not standalone permission. Then the existing comms-loss policy extinguishes a show-controlled flame and returns local authority.

## Carbide machine

`OFF → STRIKING → IGNITING → BURNING`, plus `LOW_FLAME`, `UNSTABLE`, `FLARE`, `EXTINGUISHING`.

Physical striker and P4 `IGNITE` enter the **same** machine. Failed strikes exist only as a configurable percentage and default to **0**.

Without Showduino:

- Ignition button: OFF → STRIKING → IGNITING → BURNING
- Short puff: flame reacts / recovers
- Sustained blow: EXTINGUISHING → OFF
- Fermion plays `flick.mp3` → `fire_ignite.mp3` → looping `flameloop.mp3`

No browser, Wi-Fi client, or internet is required for the physical lamp.

## Sensors

- **Mic:** non-blocking blow detector (baseline, relative rise, puff vs sustained). Not `analogRead > threshold`.
- **Light:** telemetry only. Normalized 0–100 only after a stored calibration scale. No automatic flame compensation in this pass.
- **Voltage:** raw ADC always; millivolts only when a calibration scale is stored. Never invent 5.00 V.

## Local audio

DFRobot Fermion DFPlayer Pro (DFR0768) over UART, 115200, no BUSY pin. Semantic roles map to the V1 four-file library:

| Role | File | When |
|------|------|------|
| STRIKE | `flick.mp3` | STRIKING, once |
| IGNITION | `fire_ignite.mp3` | IGNITING, once |
| BURN_LOOP | `flameloop.mp3` | BURNING / LOW / UNSTABLE / FLARE / DYING, loop |
| EMERGENCY | `emergency.mp3` | System emergency, loop |

There is no `fire_out.mp3`. Extinguish stops `flameloop.mp3` and goes silent. Missing Fermion or missing files must not block the jewel. File enumeration is **UNSUPPORTED** — firmware does not invent present/missing. This is **not** the Showduino Audio Node.

## Emergency (existing product policy — plus local audio)

Emergency forces the jewel **bright white**, interrupts theatrical audio, and loops `emergency.mp3`. Clear stops emergency audio, returns to idle/OFF, and does **not** resume the previous flame or burn loop. The node cannot locally clear a system emergency. WebUI cannot clear it either. WebUI TEST EMERGENCY is an audio commissioning check only.

## Comms-loss (existing product policy — unchanged)

- Show-controlled + GRANT keepalive stale (~8 s): extinguish, report `COMMS_TIMEOUT`, then standalone.
- Standalone local burn: flame may continue. Do not inherit Pixel-node blackout for interactive use.

## Standalone SoftAP / WebUI

SSID is `Showduino-Lamp-<logical-id>`, for example `Showduino-Lamp-LAMP-01`. Password `showduino`. Typical IP `192.168.5.1` on the ESP-NOW channel. No internet, P4, Communications Controller, Director, or external server is required.

In STANDALONE the WebUI may ignite, extinguish, set theatrical flame states, test Fermion roles, and adjust volume / brightness.

In SHOWDUINO the WebUI is commissioning / status / diagnostics. Firmware rejects theatrical and audio-test commands from the browser. Identity, blow calibration, and reboot remain available.

Sections: STATUS, LAMP, AUDIO, BLOW SENSOR, LIGHT SENSOR, VOLTAGE, BUTTON, SYSTEM.

## Future Director / Studio

Director already shows lamp online/FX via P4 `LampNodeLink`. Future: show `LAMP-01` + carbide state (`BURNING`) and faults (`LOW VOLTAGE`, `LOCAL AUDIO FAULT`, `COMMS LOST`). Do not turn Director into a second lamp admin desk.

Studio should author `LAMP-01 IGNITE` / `UNSTABLE_FLAME` / `EXTINGUISH`. SHDO v2 already compiles those tokens to `LAMP:NODE:` commands. No v3 schema.

## GPIO map

| Function | GPIO | Notes |
|----------|------|-------|
| Mic / blow | 4 | ADC1_CH3 |
| Light | 5 | ADC1_CH4 |
| Voltage | 6 | ADC1_CH5, raw until calibrated |
| Ignition button | 7 | to GND, active-LOW |
| Jewel DATA | 8 | 7 pixels |
| Fermion TX | 17 | S3 TX → Fermion RX |
| Fermion RX | 18 | S3 RX ← Fermion TX |
