# FastLED foundation — no node conversion yet

This update adds a shared, opt-in strip backend and tests. **No existing node includes or enables it.** The MOSFET LED system is explicitly excluded at the owner's request and is unchanged. Director, Audio, Lamp, Emergency and P4 outputs also keep their existing drivers in this foundation update. P4 GPIO24 Emergency signage is not an ordinary show-pixel target.

This is the first separable step towards the requested FastLED functionality, not a claim that FastLED is installed across the fleet. It is stacked on the reconciled baseline PR #51. The C3 LED-only node remains deferred.

## Adapter contract

`firmware/shared-pixel/ShowduinoLedStrip.h` uses the legacy Adafruit driver by default. The compile-time FastLED path requires an explicit fixed GPIO and implements the fitted WS2812 GRB/800kHz format. Unsupported pin/count/order/speed configuration is refused rather than silently reinterpreted. Additional chipsets, RGBW and alternate channel orders are not implemented here.

The interface covers initialization/readiness, RGB writes/reads, count, clear, brightness and per-strip refresh. The FastLED buffer stores unscaled RGB and applies brightness at output; it does not emulate Adafruit's lossy rescaling after arbitrary brightness changes. Existing engines must be assessed against that difference before adopting the adapter. A successful `begin()` establishes a buffer/controller binding, not physical-output or timing acceptance.

FastLED controllers are registered once per fixed GPIO and rebound to new buffers after the previous owner is destroyed. A second live owner of the same line is refused. Destruction detaches only the owning buffer before freeing it. Refresh calls the selected controller, never the global `FastLED.show()`, so requesting refresh on one line does not intentionally flush every registered line.

The design is adapted from PR #31 at `d298fadc33636b0737a0d24e0187ed272d8df683`, with explicit ownership, geometry and initialization checks added. The old branch's broad node replacements/default flips are not imported.

## Validation

Host tests compile both backends with warnings as errors. They exercise frame data/bounds, explicit full-white writes, initialization/buffer failures, duplicate GPIO ownership, destruction, repeated count/reallocation cycles, allocation failure/retry and isolated per-controller refresh. The vendor APIs are stubbed: these tests do not validate waveform, current, refresh timing, RF, real FastLED allocation or perceptual colour/brightness parity.

An isolated Arduino API probe compiles against the real Adafruit NeoPixel 1.15.5 and FastLED 3.10.8 libraries with ESP32 core 3.3.12, for ESP32 and ESP32-S3 and both backend choices. The pins/count in that probe are test values, not installation wiring. **It is not a node image and must not be flashed.** CI retains logs only; it does not publish probe binaries as updates.

## Next integration decisions

- Keep MOSFET indicators outside the migration.
- Review which ordinary show output to convert first, retaining current pin/count/ownership/Emergency semantics and a legacy build path.
- Extract and compare shared effect rendering without changing effect IDs, SHDO commands or show timing.
- Validate each newly connected active node's complete target build, then its physical effect/brightness/radio/audio/Emergency behavior before changing the deployed driver.
- Handle browser groups, full Director group/effect controls and P4 GPIO23 backend separately. GPIO24 retains native RMT. No clock-synchronised group-start or multi-physical-output feature is claimed by this adapter.

No original working files, boards, production assets or existing driver flags are changed by this update. No merging or flashing is implied by its draft PR.
