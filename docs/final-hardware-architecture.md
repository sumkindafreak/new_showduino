# Showduino Final Hardware Architecture

This document describes the intended Showduino hardware stack and how boards map to **roles**.

## Architectural constitution

> The Show Engine decides.  
> The Communications Engine transports.  
> The Director commands and displays.  
> The Nodes act.

### Naming

| Term | Meaning |
|------|---------|
| **Show Engine** | Software / processor role: single source of truth |
| **Stage Controller** | Physical **ESP32-P4** product that runs the Show Engine |
| **Communications Engine** | Physical **ESP32-C3** (active) providing ESP‑NOW and (planned) Wi‑Fi |
| **Director** | Physical **ESP32-S3** touchscreen operator desk |
| ~~Stage Engine~~ | **Retired** term |

Firmware folder `firmware/stage-engine-p4/` hosts the Show Engine sketch temporarily; rename planned.

---

## 1. Core system topology

### Operator desk

```text
Director ESP32-S3 (5" touch)
        |
        | ESP-NOW
        v
Communications Engine ESP32-C3
        |
        | UART 115200
        v
Show Engine on Stage Controller (ESP32-P4)
```

### Nodes

```text
Showduino Node (relay / audio / pixel / …)
        |
        | ESP-NOW
        v
Communications Engine ESP32-C3
        |
        | UART
        v
Show Engine on Stage Controller (ESP32-P4)
```

### Browser / phone (conceptual — Show Engine services)

```text
Phone / tablet / laptop
        |
        | Wi-Fi
        v
Communications Engine ESP32-C3
        |
        | to Show Engine services
        v
Web UI / Web API / WebSocket on Show Engine
```

The Director does **not** host or proxy the primary Web UI. USB is not the normal Director path.

---

## 2. Director hardware (ESP32-S3)

### Role

Human-facing control surface: command requests and status display only.

### Target board

```text
ESP32-S3 with 5 inch 800x480 capacitive touchscreen
Recommended: Sunton / similar ESP32-8048S043C or ESP32-8048S050C
PSRAM required for RGB panel framebuffer
microSD for UI assets and temporary local data
```

### Active software

```text
firmware/director-esp32-8048s050/
```

(`firmware/director-s3/` is legacy / experimental.)

### Connections (normal product)

```text
ESP-NOW only to Communications Engine
Shared RF environment / channel configuration as documented in firmware
Optional USB for flash and Serial diagnostics
```

Direct UART Director↔P4 is **not** the normal architecture (bench/service only if ever enabled).

### Director must not

- Host the primary Web UI
- Own authoritative project storage long-term
- Drive relay/node GPIOs directly

---

## 3. Stage Controller / Show Engine hardware (ESP32-P4)

### Role

Runs the **Show Engine**: authoritative show state, safety, project store (target), Web services (target), and dispatch of node work through the Communications Engine.

### Target board

```text
ESP32-P4 Stage Controller
Optional onboard ESP32-C6 radio is not the active Communications Engine in the current stack
```

### Active software

```text
firmware/stage-engine-p4/          # Show Engine (early implementation)
```

### Connections

```text
UART <-> Communications Engine ESP32-C3
Optional local DMX / I2S / pixels / IO when implemented
Optional local storage for projects and runtime assets (target)
```

### Maturity

Current P4 firmware is an **early command hub**. Full timeline, storage, DMX, pixel, audio, and Web UI are **planned**, not claimed as complete.

---

## 4. Communications Engine hardware (ESP32-C3)

### Role

Transport only: ESP‑NOW (Director + nodes), UART to Show Engine, planned Wi‑Fi for browser access to Show Engine services.

### Active board / software

```text
ESP32-C3 SuperMini (or equivalent)
firmware/c3-supermini-espnow-bridge/
```

### Must not

- Run the show timeline
- Own authoritative show state
- Pretend unimplemented pixel/audio routes succeeded

### Alternate / non-active bridges

```text
firmware/p4-c6-espnow-bridge/    experimental / incomplete
firmware/espnow-bridge/          legacy scaffold
```

---

## 5. Relay nodes

### Role

Act on absolute relay commands near the prop. Report completion separately from command acceptance.

### Active software

```text
firmware/relay-node-esp32/
```

### Hardware options

```text
ESP32 + 4 or 8 relay module
ESP32-C3 + driver board
Custom Showduino relay PCB
```

### Behaviour

- Boot with relays OFF
- Accept absolute ON / OFF / timed pulse (not distributed TOGGLE at application level)
- Safe state on emergency
- Remain safe until valid clear per policy
- Application addressing by logical device ID (transport may use MAC internally until ID map lands)

---

## 6. Future node families (hardware direction)

Unchanged as hardware intent; all speak ESP‑NOW to the Communications Engine:

- Audio nodes (e.g. ESP32-C3 + DFPlayer / I2S)
- Pixel / FastLED nodes
- Sensor, motor, environmental, combo prop nodes
- R3-style puzzle terminals as optional integrated props

Command examples in older drafts remain illustrative. Placeholder routes must not return false success in firmware.

---

## 7. Legacy / development hardware

Useful for labs; **not** the active product path:

| Hardware | Classification |
|----------|----------------|
| CYD 2.8″ (`firmware/controller-cyd/`) | Legacy — archive candidate |
| Arduino Mega executor (`firmware/executor-mega/`) | Legacy — archive candidate |
| Touch / SD probe sketches | Diagnostic |

---

## 8. Power architecture

Unchanged engineering rules:

```text
5V  — logic boards, many relay modules, pixels, DFPlayers
12V — props, solenoids, lamps, motors as required
3.3V — ESP32 logic only
```

- Shared GND for all signal companions
- No 5V into ESP32 GPIO
- Fuse high-current groups
- Inject pixel power properly; series resistor on data; bulk capacitance at strip
- Emergency policy disables dangerous energy as designed per venue
- Nodes boot outputs OFF

---

## 9. Minimum demo that matches this architecture

```text
1x ESP32-S3 5" Director          firmware/director-esp32-8048s050/
1x ESP32-C3 Communications Engine firmware/c3-supermini-espnow-bridge/
1x ESP32-P4 Stage Controller      firmware/stage-engine-p4/
1x ESP32 Relay Node               firmware/relay-node-esp32/
Shared 5V/12V power + GND
Emergency stop path exercised end-to-end
```

Optional later: audio node, pixel node, browser via Communications Engine Wi‑Fi to Show Engine services.

---

## 10. Product family naming

```text
Showduino Director
Showduino Stage Controller   (runs Show Engine)
Showduino Communications Engine
Showduino Relay Node 4 / 8
Showduino Audio Node
Showduino Pixel Node
Showduino Prop Node
```

Avoid shipping new materials that say “Stage Engine.”
