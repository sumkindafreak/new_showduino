# Audio RGB/output boundary — Stage B candidate

This candidate implements the behavior-preserving boundary from the Audio design after Stage A (#56). It retains the legacy effect renderer and actual native RMT transport. It does not enable FastLED rendering, register a FastLED controller or change MP3/playback, protocol, counts, settings or Emergency policy.

## Boundary and ownership

`AudioPixelFrame.h` separates two operations:

1. `audioPixelPackRgbToGrb` accepts RGB888 bytes and caller-owned output capacity. It checks non-null buffers, nonzero/triplet-aligned size and capacity before writing, then packs G/R/B explicitly. It applies no brightness or gamma. Buffers must be disjoint or exactly aliased; partial overlap is unsupported.
2. `audioPixelWriteRgbFrame` requires a complete frame matching the existing buffer's pixel count and full driver brightness. It packs into the Audio-owned NeoPixel buffer and returns the actual `AudioPixelOutput::write` result. The two existing Audio callers configure that buffer as NEO_GRB+NEO_KHZ800; alternate orders/RGBW are outside this wrapper's contract. It does not infer or validate a different vendor buffer colour order at runtime.

The Adafruit objects are retained as their existing buffers, with the same construction, lifetime, GPIO and release sequence. There is no new buffer or per-frame heap allocation. Ordinary show brightness remains applied once by the renderer; Emergency supplies full white before packing. The status caller presents one RGB triplet and updates its cached successful colour/time only after a true transport result. The fitted status count stays one.

The show engine's existing `writeFrame` now delegates to this boundary. The status policy still selects exactly the same packed RGB colour and presents its three components to the same boundary. `AudioPixelOutput.cpp/.h`, their native encoding, allocation, timeout, errors and failure returns remain unchanged. Renderer math, commands and ownership code remain unchanged.

The full-driver-brightness guard and size/null/capacity guards are new rejection checks at this boundary. They do not affect the supported current callers, which keep driver brightness 255 and use exactly sized RGB/GRB frames. Unsupported input is rejected before transport; this is not a claim of parity for arbitrary malformed direct calls.

## Comparison evidence

The test runner compiles both actual original implementations from Stage A commit `2d896781274fa21a34d421e9e6fd56bc897fe70e` and the candidate with identical host clocks, deterministic seeds, commands and fault injections. Both use real Adafruit NeoPixel 1.15.5 buffers and the unchanged native encoder; hardware calls are doubled.

For the show engine, the comparison includes all existing effect IDs, 324 sampled frames, brightness/count/rebind cases, Emergency and failure/retry. It compares complete attempt/symbol traces, sampled state, command responses, config writes, serial messages and vendor initialization/release counts. Status comparison additionally records successful-colour/time cache values and test flags during failures, retries, refreshes and Emergency sources.

Local exact original/candidate comparisons pass. Direct boundary tests cover GRB order, null/size/capacity/alignment rejection before partial writes, exact in-place packing, brightness guard and propagation of a failed native write. CI repeats the tests normally and under AddressSanitizer/UndefinedBehaviorSanitizer with leak checks, and compiles complete active Audio reference firmware. Full traces are saved as CI artifacts; hashes are evidence for a run, not a cross-platform floating-point or line-ending compatibility guarantee.

These synthetic tests do not measure real RMT waveform, resource ownership, CPU timing, MP3/I2S/SD/radio coexistence, board heap/stack or physical Emergency latency. Playback code is untouched, but playback-controller behavior is not proved by LED tests.

## Acceptance and next stage

This is a reviewable implementation candidate, not a deployed release. Exact installed board/core/partition/library settings and rollback image must be reviewed before any upload. Physical validation still requires the 22-second full-node show, live MP3/WAV and radio activity, repeated count/reboot checks, both outputs' Emergency white and clear/no-resume behavior, failure recovery and measured resource use.

After the boundary passes review and required validation, an opt-in FastLED renderer can fill the same RGB frame without replacing native transport. New effect IDs/parameters and intentional visual differences require explicit mapping and tests. MOSFET LEDs remain excluded, C3 LED-only deferred, Lamp/Director physical acceptance pending. No merge or upload is performed by this stage.
