# Audio FastLED integration design — retain the working wire transport first

Status: design for review, no Audio firmware changes. Applies to the current `0.5.0-mp3-rc2` ESP32-A1S baseline, not the older Audio implementation from PR #31. MOSFET LEDs and the deferred C3 LED-only node are outside this design.

## Decision

Separate rendering from transport. Introduce FastLED colour/palette/effect capabilities behind an Audio-specific opt-in renderer while keeping `AudioPixelOutput` as the initial wire transport. FastLED rendering does not require registering a FastLED controller or calling `FastLED.show()`. A later FastLED wire-driver experiment is an independent opt-in change only if justified by requirements and measured coexistence.

This removes an unnecessary dependency between FastLED features and replacing a working RMT transport. It does not claim all Pixel Engine 2.0 features are implemented: effect IDs, segment/group authoring and scheduling still require explicit adaptation and validation.

## Current boundaries

| Line | Buffer and policy owner | Wire output | Protected behavior |
|---|---|---|---|
| GPIO22 show pixels | AudioPixelEngine: canonical RGB `sFrame`, segments, brightness, count, ownership and Emergency state; Adafruit buffer currently packs GRB | Independent `sShowOutput` instance of AudioPixelOutput | Default ten; preserve valid larger saved count; existing max and command checks; white overrides brightness, effects, tests and blackout |
| GPIO5 status | AudioStatusPixel selects one RGB pixel from node/owner/recording/fault/test/locate state | Independent `sStatusOutput` instance | Fixed count one; Emergency white outranks fault/locate/test; failed writes do not advance cached successful colour/time |

AudioPixelOutput owns an RMT channel and symbol buffer, validates byte count, writes with an 80 ms timeout and returns success/failure. It encodes already-packed wire bytes; it must not apply colour order or brightness again. It currently allocates `(wireByteCount * 8 + 1) * sizeof(rmt_data_t)` bytes for symbols. Runtime memory must include this allocation, RGB/GRB buffers, MP3 decoding, SD, tasks and radio; static RAM build totals do not cover these costs.

## Proposed renderer/packing contract

1. Keep policy in AudioPixelEngine/AudioStatusPixel. Authoritative Emergency handling remains outside effect math. Do not move ownership, persistent count/brightness or playback logic into a driver.
2. A renderer fills a caller-owned RGB888 frame of the validated pixel count. Legacy and future FastLED implementations receive the same segment IDs/parameters, clock and seed inputs. Renderer selection defaults to legacy and is not saved in NVS.
3. A packing function converts RGB triplets to the fitted GRB wire order with explicit length/capacity checks. It does not change brightness. Ordinary brightness is applied exactly once by the established renderer policy; Emergency supplies full `255,255,255` directly.
4. `writeGrb(bytes,length)` passes the packed buffer to the existing AudioPixelOutput and preserves its actual return value. The current status caller updates its successful-write cache only after true. Show-output errors remain observable through the existing return/log path.
5. Use caller-owned buffers sized at initialization, not per-frame allocation. Preserve the existing shutdown/rebind/count-change sequence and release RMT ownership before rebuilding the corresponding line. Do not disturb the other line's controller or buffer.
6. Keep status and show refresh separate. No global FastLED refresh; no controller registration is needed in this initial design. Do not force the one-pixel status line through programmable default-ten count rules.

The current shared ShowduinoLedStrip adapter is not used for this boundary: it has no raw-GRB buffer interface and its `show()` returns no transport result. Replacing a boolean RMT result with unconditional success after `show()` would weaken existing failure semantics.

## Stages and reviewable changes

| Stage | Change | Required evidence before next stage |
|---|---|---|
| A | Test the actual existing RGB-to-GRB/RMT behavior, counts, independent lines, retry/cache and Emergency policy without changing drivers | Golden byte/symbol traces; rejected lengths; allocation/init/write failures; current Audio complete build |
| B | Introduce the explicit renderer/packer boundary with the same legacy renderer and native transport | Byte-identical legacy traces across effects/segments/brightness/counts, Emergency assertion/clear, locate/tests and ownership; MP3/SD/I2S code unchanged; complete build and heap accounting |
| C | Add opt-in FastLED palette/effect math on RGB frames, retaining native transport | Existing IDs preserve meaning; any new IDs/versioning mapped explicitly; deterministic comparisons for equivalent effects and documented intentional new output; full build plus runtime validation |
| D | Optional separate FastLED transport experiment | Per-line readiness/write-result contract designed honestly, GRB/brightness parity, no resource ownership conflict, timed radio/I2S/MP3 coexistence, recovery and rollback evidence |

Do not merge stages B–D into one all-node replacement. The default remains the known working renderer/transport until separately reviewed and accepted. A FastLED-rendered candidate using native RMT must be labelled accurately; it is not a commissioned FastLED wire-driver conversion.

## Software tests to prepare

- Compare exact packed GRB bytes and native RMT symbols for black, RGB primaries, white, mixed colours, minimum ten and representative larger counts through the supported maximum. Verify bounds and reset framing; compare to actual current code rather than a rewritten model alone.
- Test separate status/show instances with alternating refreshes, count reinitialization, failed init/allocation/write and recovery. Verify one line's refresh/rebind cannot flush or detach the other.
- Exercise current effect/segment/global brightness interactions and zero brightness; Emergency must force both outputs white through normal protocol requests, then clear without automatic playback/show resume. Preserve existing policy distinctions such as boot tests and locate.
- Preserve volume, MP3 interruption/yielding, SD access discipline and current protocol acknowledgements. Do not equate a renderer test with playback acceptance.

## Hardware acceptance, still pending

Use the owner's 22-second full-node show and known rollback image. Compare native legacy and each candidate under MP3/WAV playback, pause/resume/stop, volume changes, ESP-NOW traffic and web/SD activity. Measure free/minimum heap, task stacks, decode timing, RMT durations/timeouts, visible pixel jitter, missed acknowledgements and Emergency-to-white latency at both lines. Exercise count changes/reboots and repeated assertion/clear with no automatic resume. Review exact installed partition/core settings before any upload; neither a green reference build nor this design authorizes flashing.
