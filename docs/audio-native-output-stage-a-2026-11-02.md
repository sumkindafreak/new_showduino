# Audio native output — Stage A regression evidence

This batch implements the first software-validation stage of `audio-fastled-transport-design-2026-11-02.md`. It changes tests, CI and documentation only. Audio firmware, MP3/playback, native RMT transport, drivers, protocol and saved configuration remain unchanged. It does not enable FastLED rendering or transport.

## What executes

The tests compile the actual production `AudioPixelOutput.cpp`, `AudioPixelEngine.cpp` and `AudioStatusPixel.cpp`, plus the real unmodified Adafruit NeoPixel 1.15.5 `.cpp`/header for RGB-to-GRB buffers. The vendor source is read from a separately installed pinned library, not copied into this repository.

Arduino time, GPIO, Serial, selected String operations, random values, node config/state and RMT hardware calls are doubled. Vendor ESP32 hardware entry points `espInit` and `espShow` are doubled; real vendor buffer construction, colour storage, `getPixels()` and destruction execute. A nonempty vendor `show()` call is forbidden by the harness: production data must use the actual native AudioPixelOutput encoder. Zero-length vendor release calls are allowed. This does not validate the vendor's real ESP-IDF initialization/resource release or RMT peripheral implementation.

The RMT double records symbols, channel initialization/deinitialization, configured frequency, timeout and explicit success/failure. Symbol fields are host representations, not the board ABI. Timeouts do not wait and elapsed CPU time is not measured. The deterministic test RNG and minimal String double do not reproduce all Arduino random/parser semantics.

## Coverage

| Test | Production behavior checked |
|---|---|
| Output and real vendor buffer | Mixed RGB packs to exact GRB; 10/30/512-pixel lines and repeated rebind; all 256 byte patterns, MSB-first; 10 MHz configuration; bit-one 8/4 ticks, bit-zero 4/8 ticks; low reset 600/600 ticks; write timeout argument 80 ms |
| Output failure/ownership | Zero/oversized begin, uninitialized/null/wrong-length writes, symbol allocation and RMT init failure, write failure return and rate-limited logging, retry/recovery, idempotent end and separate GPIO5/GPIO22 lifetimes/refresh |
| Actual show engine | Missing/below-minimum/out-of-range stored counts fall back to ten, valid 30/512 preserved; frame -> real GRB buffer -> actual encoder for every current effect ID, 324 sampled frames; ordinary brightness 0/1/127/255; init/allocation/write failure cleanup/retry; count reinitialization |
| Show Emergency | Full white at brightness zero, locate cancellation, normal command rejection, blackout cannot displace white, periodic white refresh/retry after failed write, clear produces black and inactive effect flags; subsequent service stays black without a new start |
| Actual one-pixel status | Init failure/recovery; failed writes do not advance successful-colour/time cache and retry; successful writes suppress unnecessary refresh until 250 ms; playing/paused colours; node-state/owner-mode/show-engine Emergency sources outrank fault/locate/test/recording; status returns to current state on clear |

The show-engine test checks transmitted frames and actual internal running flags; it does not exercise the playback controller and cannot establish that audio itself stops/resumes correctly. That remains a separate existing playback/physical gate.

## Observed contracts to preserve or review separately

- `AudioPixelOutput::begin` tears down an existing binding before validating a new size. A failed/invalid re-begin does not retain the old working channel. The tests record this existing behavior rather than changing it.
- The existing `audioPixelEngineActiveSegments()` getter counts configured nonempty ranges, not running effect flags. It can report one after Emergency clear while the line is correctly black and effects are stopped. The no-resume test checks `seg.active` and transmitted black frames. This naming/status mismatch is recorded for separate review; no status API change is made here.
- Status uses one physical pixel. Its failure cache must represent successfully transmitted colour/time, not the last attempted write. A failed attempt is not silently treated as success.
- The raw output boundary validates byte length, not three-byte alignment. Tests use valid RGB line sizes for policy checks and a 256-byte raw payload for exhaustive bit-pattern coverage. Do not interpret raw byte acceptance as supported partial-pixel commissioning.

CI runs normal host builds and AddressSanitizer/UndefinedBehaviorSanitizer with leak checking. A clean run supports the tested host paths; it is not proof of board memory use, thread safety or all possible failure paths. Full unchanged Audio reference firmware compiled in PR #54; this batch has no firmware diff requiring a new image.

## Next step

Use these tests as the comparison gate for Stage B: extract an explicit renderer/GRB-packing/native-output boundary while retaining the same legacy renderer and transport. Compare the original and candidate code on identical clocks/seeds/commands, preserving bytes, acknowledgements, counts, cleanup and failure semantics. FastLED rendering comes in a later opt-in batch. Physical MP3/I2S/SD/radio timing, actual RMT ownership, Emergency latency, heap/stack and the owner's 22-second show still require hardware validation before acceptance.

MOSFET LEDs remain excluded, C3 LED-only deferred, Lamp/Director FastLED candidates not bench-accepted. No merge or hardware upload is performed by this validation stage.
