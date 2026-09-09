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
2. C3 Lamp Node
3. C3 Pixel Node
4. MOSFET Node
```

### 1. Audio Node

Current first production specialist node. Firmware exists (0.4.0) with P4 GRANT ownership and standalone SoftAP WebUI. Hardware commissioning still required.

### 2. C3 Lamp Node

Second specialist node. Replaces the retired Relay Node product role on the Director (Page 04 / Home footer). Firmware lives in `firmware/c3-lamp-node/`. Carbide / theatrical lamp FX; not a 1:1 mapping of old `RELAY:n:ON` commands.

P4 links via `ROUTE:LAMP:` / `LampNodeLink` (Audio Node parity).

### 3. C3 Pixel Node

Follows the Lamp Node.

It should reuse the common Showduino pixel-effect vocabulary in:

```text
protocol/showduino_pixel_fx.h
```

The aim is for `LIGHTNING`, `FIRE`, `FLICKER`, etc. to mean the same thing on the P4 local line and the C3 Pixel Node.

Every pixel-capable node must implement the global Showduino emergency rule:

> EMERGENCY = ALL PIXELS BRIGHT WHITE.

No local FX or segment may override it.

### 4. MOSFET Node

Digital on/off and PWM / dimming specialist. Not a revival of the Relay Node product.

The existing `firmware/relay-node-esp32/` tree is retained only as legacy/reference source. Do not advance it as the current node design.

## DMX

DMX remains explicitly parked and out of scope until it is deliberately reopened. Existing experimental/foundation files may remain in the repository, but they are not part of this rollout.
