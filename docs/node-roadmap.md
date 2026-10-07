# Showduino Specialist Node Roadmap

Status: source-reviewed rollout baseline, 2026-10-07.

This document records rollout order only. It does not authorise work on later node firmware before the relevant hardware is ready.

## Immediate platform work

Before moving to the next specialist node, bring up and bench-test the P4 local pixel system:

```text
GPIO24 emergency/signage pixels
  NORMAL    → first pixel of every 10-pixel sign GREEN, remaining nine OFF
  EMERGENCY → every pixel WHITE

GPIO23 Show Pixel Line
  NORMAL    → segmented theatrical FX
  EMERGENCY → every pixel WHITE
```

See [`audio-pixel-engine.md`](audio-pixel-engine.md).

## Specialist node order

```text
1. Audio Node
2. S3 Lamp Node
3. C3 Pixel Node
4. C3 Emergency Node
5. MOSFET Node
```

### 1. Audio Node

Current first production specialist node. Firmware exists (0.4.3) with P4 GRANT ownership and standalone SoftAP WebUI. Hardware commissioning still required.

### 2. S3 Lamp Node

Second specialist node. Replaces the retired Relay Node product role on the Director (Page 04 / Home footer). Production firmware lives in `firmware/s3-lamp-node/`. Historical C3 Super Mini OLED firmware is retained in `firmware/c3-lamp-node/`.

This is an interactive carbide-lamp practical (jewel flame, striker, blow-to-extinguish, local Adafruit Audio FX UART WAV effects). Firmware **0.4.3** is both a standalone SoftAP carbide lamp and a managed Showduino node. It is not a Pixel Node and not a second Audio Node. Production GPIOs are confirmed. See [`s3-lamp-node.md`](s3-lamp-node.md).

P4 links via `ROUTE:LAMP:` / `LampNodeLink` (Audio Node parity).

### 3. C3 Pixel Node

**Classification:** firmware implemented / hardware test required.

Firmware lives in `firmware/c3-pixel-node/`. See [`c3-pixel-node.md`](c3-pixel-node.md).

It reuses the common Showduino pixel-effect vocabulary in `protocol/showduino_pixel_fx.h`. `LIGHTNING`, `FIRE`, `FLICKER`, etc. mean the same thing on the P4 local line and the C3 Pixel Node.

Every pixel-capable node implements the global Showduino emergency rule:

> EMERGENCY = ALL CONFIGURED PIXELS BRIGHT WHITE.

No local FX, segment, Locate, or WebUI test may override it.

### 4. C3 Emergency + Pixel Node

**Classification:** firmware implemented / GPIO unconfirmed / Pixel capability on GPIO2.

Distributed wireless Emergency Button stations (`ESTOP-01` …). Momentary pushbutton, ASSERT ONLY, never clear. SSD1306 OLED status. Same physical peer also hosts the **full Showduino Pixel Controller** on GPIO2 (`ESTOP:NODE:PIXEL:` — no fake LED identity). Shared engine: `firmware/shared-pixel/`. Standalone C3 Pixel Node remains supported. P4 GPIO25 remains the independent main Emergency path. Offline in V1 is a safety fault, not automatic global emergency. Update one station at a time: ESTOP-01 → reboot → healthy+linked → ESTOP-02. See [`emergency-node.md`](emergency-node.md).

### 5. MOSFET Node

**Classification:** SOFTWARE IMPLEMENTED / HARDWARE PIN MAP DEFINED / PHYSICAL OUTPUT VALIDATION REQUIRED.

ESP32_MOS_X4 / 303E32NMOS4 four-channel digital/PWM powered-output specialist (`MOSFET-01` … `MOSFET-08`). Firmware `0.1.3` in `firmware/mosfet-node-esp32/`. Fail-safe ALL OFF; no stale restore. SoftAP commissioning. Studio Powered Output compiles to `MOSFET:NODE:…` timeline cues. GPIO map software-defined (OUT1–4 → 16/17/26/27, LED 23) and four identifier NeoPixels on GPIO25, with `SHOWDUINO_MOSFET_GPIO_VERIFIED=0` until Toby benches his board.

Not a revival of the Relay Node product. `firmware/relay-node-esp32/` remains legacy/reference only.

## DMX

DMX remains explicitly parked and out of scope until it is deliberately reopened. Existing experimental/foundation files may remain in the repository, but they are not part of this rollout.
