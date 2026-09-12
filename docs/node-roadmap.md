# Showduino Specialist Node Roadmap

Status: current planning baseline, 2026-09-07.

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
4. MOSFET Node
```

### 1. Audio Node

Current first production specialist node. Firmware exists (0.4.1) with P4 GRANT ownership and standalone SoftAP WebUI. Hardware commissioning still required.

### 2. S3 Lamp Node

Second specialist node. Replaces the retired Relay Node product role on the Director (Page 04 / Home footer). Production firmware lives in `firmware/s3-lamp-node/`. Historical C3 Super Mini OLED firmware is retained in `firmware/c3-lamp-node/`.

This is an interactive carbide-lamp practical (jewel flame, striker, blow-to-extinguish, local Fermion FX). Firmware **0.3.3** is both a standalone SoftAP carbide lamp and a managed Showduino node. It is not a Pixel Node and not a second Audio Node. Production GPIOs are confirmed. See [`s3-lamp-node.md`](s3-lamp-node.md).

P4 links via `ROUTE:LAMP:` / `LampNodeLink` (Audio Node parity).

### 3. C3 Pixel Node

**Classification:** firmware implemented / hardware test required.

Firmware lives in `firmware/c3-pixel-node/`. See [`c3-pixel-node.md`](c3-pixel-node.md).

It reuses the common Showduino pixel-effect vocabulary in `protocol/showduino_pixel_fx.h`. `LIGHTNING`, `FIRE`, `FLICKER`, etc. mean the same thing on the P4 local line and the C3 Pixel Node.

Every pixel-capable node implements the global Showduino emergency rule:

> EMERGENCY = ALL CONFIGURED PIXELS BRIGHT WHITE.

No local FX, segment, Locate, or WebUI test may override it.

### 4. MOSFET Node

Digital on/off and PWM / dimming specialist. Not a revival of the Relay Node product.

The existing `firmware/relay-node-esp32/` tree is retained only as legacy/reference source. Do not advance it as the current node design.

## DMX

DMX remains explicitly parked and out of scope until it is deliberately reopened. Existing experimental/foundation files may remain in the repository, but they are not part of this rollout.
