# Audio FastLED RGB frame candidate — C1

Owner decision: preserve existing effects exactly first. This candidate adds an
optional real FastLED 3.10.8 `fl::CRGB` render buffer without changing effect
calculations, names, IDs, timing, brightness policy or command handling.

`SHOWDUINO_AUDIO_FASTLED_RGB_FRAME` defaults to 0. The default remains firmware
`0.5.0-mp3-rc2`; explicit flag 1 identifies `0.5.1-rc1`. Other values fail compilation.
This is the first frame integration stage, not completion of Pixel Engine 2.0,
new palettes, groups, or a FastLED wire transport.

The show line remains GPIO22, initially 10 pixels with larger valid saved counts
preserved; GPIO5 remains one status pixel. RGB fields are explicitly packed into
the existing Audio-owned GRB buffer and submitted through AudioPixelOutput.
There is no FastLED controller registration, global show(), or per-frame heap
allocation. The native RMT driver, status renderer and MP3 source are unchanged.
Emergency still supplies full white independently of saved brightness; clear
leaves the show line black with effects stopped.

## Software evidence

The host runner compiles real FastLED RGB headers and real Adafruit NeoPixel
1.15.5 buffer code, with hardware/time/config doubles. Vendor ESP32 packing code
is compiled separately so FastLED uses the host's actual integer widths in host
tests. No library source is modified or replaced by a fake RGB type.

Original Stage A, current default and opt-in candidate produce identical full
engine traces: 324 sampled frames across all 25 effect IDs, 1,195,876 trace bytes,
SHA256 `b2b53eab70727caba09ed7041233bc2867c8f6526b17ba82d2d511e53ac868e0`.
This includes encoded symbols, attempted writes, command replies, state and logs.
The status comparison also remains exact. Tests cover native failure/retry,
brightness, Emergency priority, 10/30/512 counts, release/rebind and failed
FastLED frame allocation followed by successful retry. Sampling is regression
evidence, not proof for every possible command sequence or physical timing.

CI builds both full Audio variants and runs normal plus address/undefined
behavior/leak checks. Acceptance requires those checks to pass at the reviewed
commit; build and test results are recorded separately from bench acceptance.

## Review and bench sequence

Keep the supplied flashed baseline unchanged. Review this isolated draft after
its parent stack; do not merge or flash automatically. Preserve baseline build,
firmware and show files for rollback. First compare default firmware on the
22-second full-node scenario, then explicitly enable this Audio candidate only.
Compare all commissioned effects/colours/brightness and Emergency stop/clear,
including playback during maximum fitted pixel activity. Verify MP3 continuity,
status priority, reconnects and repeated starts. Record installed board/build,
pixel count, show file, results and owner acceptance before expanding scope.
No hardware acceptance is claimed by the software checks.
