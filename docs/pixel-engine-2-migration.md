# Pixel Engine 2.0 — opt-in FastLED foundation

This is a development branch. Neither the P4 emergency RMT line nor any deployed node was modified or flashed.

## What changed

- P4 GPIO23 and the C3 Pixel/Emergency Nodes now share **one byte-for-byte identical legacy FX rendering function** in `ShowduinoPixelEffects.inc`.
- Common RGB/HSV and animation timing maths live in `ShowduinoPixelMath.inc`.
- C3 shared pixel engine has a **compile-time choice** between the existing Adafruit NeoPixel driver (default) and FastLED (experimental). Existing `.shdo`, pixel commands, FX IDs and brightness maths remain unchanged.
- `PIXEL:COUNT` followed by `PIXEL:INIT` is still supported. FastLED's controller is registered only once then rebound on length changes.
- Emergency white override continues to use the pre-existing Showduino rendering and authority checks.

## Enable FastLED on a C3 bench board

Add `#define SHOWDUINO_PIXEL_USE_FASTLED 1` in that sketch's `BoardConfig.h` before the shared engine is included. Install FastLED in Arduino IDE. The default colour order is `GRB` and chipset `WS2812` on the unchanged data GPIO2. Optionally define `SHOWDUINO_PIXEL_FASTLED_ORDER` for another supported order. Set the switch back to `0` for immediate legacy build behaviour.

## Validation checklist (NOT yet confirmed on hardware)

1. Compile each default C3 firmware without adding FastLED; verify pixel commands and emergency whites.
2. Enable FastLED for the C3 Pixel Node and compile against the installed ESP32 core. Check startup, web ownership, OLED and ESP-NOW.
3. Run `PIXEL:COUNT:10`, `PIXEL:INIT`, `PIXEL:SOLID:255:0:0`, then repeat with 30 LEDs and ensure no controller leaks.
4. Compare all original FX at identical brightness, speed, segment settings and count.
5. Run the white emergency override on both C3 Pixel and C3 Emergency boards; verify show commands are rejected during the latch and behaviour after clear.
6. Confirm memory, refresh time, radio stability and power limits on larger strips.

P4 FastLED, additional physical GPIO outputs per node, clock-scheduled cross-node start and native Director pixel editing are **not implemented by this foundation patch**. Do not mistake command fan-out for frame-perfect synchronisation.

## Studio group editor (branch prototype)

The Outputs screen discovers the already-supported P4 show line and C3 Pixel Node identities, lets you select members, save named group memberships in that browser, and send coordinated segment configuration / start and blackout commands. It deliberately keeps the existing \`PIXEL:\` and \`PIXEL:NODE:<id>:\` command syntax unchanged for compatibility with existing shows.

**Limitations:** Group launches use sequential gateway commands (not sub-frame sync), each selected segment range must fit every chosen physical line, groups are browser-local, and additional GPIO lines on a single node are not exposed. The emergency signage line is excluded and the controls are locked during emergency.

## Director starter controls (branch prototype)

The existing Nodes → Pixel card exposes P4 show line BLACKOUT/LINE TEST when the Stage is linked, and LOCATE for the P4-reported first Pixel Node. All new actions are blocked while the Director has emergency locked. Node locate does not guess an ID and never touches GPIO24 emergency signage.

The native Director still needs a full dynamic line picker and group/effect assignment editor before claiming complete parity with Studio; this increment intentionally does not introduce unproven multicast/scheduling logic.

## All-node backend coverage (2026-10-13)

On this **experimental branch**, FastLED has been selected for C3 Pixel, C3 Emergency Node show pixels, C3 Lamp, S3 Lamp, Audio Node, MOSFET identifier pixels and Director ambient pixels. A shared FastLED strip API adapter preserves each node's original animation/priority code.

P4 Stage Engine **GPIO23 show pixels** now also have a compile-time FastLED backend (`SHOWDUINO_SHOW_PIXEL_USE_FASTLED=1`), **deliberately disabled by default** until real P4 hardware verification. It disables FastLED's RMT driver to avoid competing with the P4's separate GPIO24 emergency native RMT channel. Validate PARLIO/SPI output and native RMT signage together before turning on. **GPIO24 signage remains native RMT** by design: it is the protected safety fallback, not an ordinary stage-effects output.

The library dependency now includes FastLED on feature-branch builds of these nodes. Standard production `main` stays untouched. None of the new all-node FastLED configurations is claimed to have compiled or passed hardware commissioning.

## FastLED allocation checks

All newly converted node engines check their FastLED controller readiness after `begin()` rather than falsely claiming READY after an allocation failure. This includes the C3/S3 Lamp, Audio, MOSFET and Director ambient pixel engines. The Director's internal `scale8` helper was renamed `ambientScale8` to avoid collisions with FastLED's global colour maths. The existing default P4 native RMT safety path remains unaffected.
