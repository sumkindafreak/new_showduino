# Showduino S3 Lamp Node — carbide lamp

Product: Showduino **1.0.0-rc.1**. Lamp firmware **0.4.0**. Protocol **1.0**. SHDO package **v2**.

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

Physical striker, optional motion IGNITE, and P4 `IGNITE` enter the **same** machine. Failed strikes exist only as a configurable percentage and default to **0**.

The Jewel starts **black** (no boot / Wi-Fi / ownership colour). A press is one IGNITE event: flint spark (one or two pixels, ~50–150 ms) → dark gap → catch (~1–2 s, carbide blue-white → pale warmth) → living procedural flame. Short puff recoils and recovers. Sustained blow collapses to black and silence. Emergency is immediate bright white + `emergency.mp3`. PIXEL IDENTIFY walks pixels 0–6 for Jewel mapping; it is blocked in SHOWDUINO and emergency.

Without Showduino:

- Ignition button: OFF → STRIKING → IGNITING → BURNING
- Short puff: flame reacts / recovers
- Sustained blow: EXTINGUISHING → OFF
- Fermion plays `flick.mp3` → `fire_ignite.mp3` → looping `flameloop.mp3`

No browser, Wi-Fi client, or internet is required for the physical lamp.

## Sensors

- **Mic:** non-blocking blow detector (baseline, relative rise, puff vs sustained). Not `analogRead > threshold`.
- **Light:** telemetry only. Normalized 0–100 only after a stored calibration scale. No automatic flame compensation in this pass.
- **Voltage:** operator-confirmed maximum is 5.00 V on the measured rail / sensor. GPIO6 stays ≤ 3.3 V. Default scale is 12-bit full scale → 5000 mV. One-point cal can store the current ADC as 5.00 V.

## Local audio

DFRobot Fermion DFPlayer Pro (DFR0768) over UART, 115200, no BUSY pin. Init is non-blocking: `AT`, `AT+FUNCTION=1` (MUSIC), `AT+AMP=ON`, volume. Play uses `AT+PLAYFILE=/name.mp3` and wiki PLAYMODE 1 (repeat one) / 3 (play once). Semantic roles map to the V1 four-file library:

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

SSID is `Showduino-Lamp-<logical-id>`, for example `Showduino-Lamp-LAMP-01`. Password `showduino`. Typical IP `192.168.5.1` on the ESP-NOW channel. Join that 2.4 GHz SoftAP, then open `http://192.168.5.1` (not https, not a hostname). No internet, P4, Communications Controller, Director, or external server is required.

In STANDALONE the WebUI may ignite, extinguish, set theatrical flame states, test Fermion roles, run PIXEL IDENTIFY, and adjust volume / brightness / flame activity / flicker / ignition speed.

In SHOWDUINO the WebUI is commissioning / status / diagnostics. Firmware rejects theatrical and audio-test commands from the browser. Identity, blow calibration, and reboot remain available.

Sections: STATUS, LAMP, AUDIO, BLOW SENSOR, LIGHT SENSOR, VOLTAGE, BUTTON, MOTION, SYSTEM. Motion action defaults to DISABLED. Theatrical motion is ignored until the GPIO15 sensor is physically confirmed.

## Future Director / Studio

Director already shows lamp online/FX via P4 `LampNodeLink`. Future: show `LAMP-01` + carbide state (`BURNING`) and faults (`LOW VOLTAGE`, `LOCAL AUDIO FAULT`, `COMMS LOST`). Do not turn Director into a second lamp admin desk.

Studio should author `LAMP-01 IGNITE` / `UNSTABLE_FLAME` / `EXTINGUISH`. SHDO v2 already compiles those tokens to `LAMP:NODE:` commands. No v3 schema.

## GPIO map

| Function | GPIO | Notes |
|----------|------|-------|
| Mic / blow | 4 | ADC1_CH3 |
| Light | 5 | ADC1_CH4 |
| Voltage | 6 | ADC1_CH5, 5.00 V full scale |
| Ignition button | 7 | to GND, active-LOW |
| Jewel DATA | 8 | 7 pixels |
| Motion | 15 | digital, 3.3 V only; sensor physically unconfirmed |
| Fermion TX | 17 | S3 TX → Fermion RX |
| Fermion RX | 18 | S3 RX ← Fermion TX |

This physical board talks over a CH343 USB-UART on UART0. Flash and monitor with `CDCOnBoot=default` so application `Serial` stays on that UART, not native USB CDC:

```text
arduino-cli compile --fqbn "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=default,FlashSize=8M,PSRAM=disabled,PartitionScheme=default_8MB" firmware/s3-lamp-node/ShowduinoS3LampNode
```
