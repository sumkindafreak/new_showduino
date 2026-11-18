# Active LED integration dependencies — software preparation

Owner instruction: S3 Lamp FastLED has not been tested on hardware; continue software preparation. MOSFET LEDs remain excluded. The standalone C3 LED-only node remains deferred. This batch changes CI and documentation only, with no firmware edits or driver conversion.

## Output evidence and safe integration order

| Output | Current source evidence | Dependency before any driver conversion | Disposition |
|---|---|---|---|
| S3 Lamp GPIO8, seven-pixel fitted Jewel | PR #53 keeps the renderer and default Adafruit driver; opt-in FastLED builds; 918 synthetic samples match reference | Physical colour/brightness, flame, radio, Audio FX and Emergency validation with rollback | Implemented/compiled candidate; not bench-tested or accepted |
| Director GPIO17, two fitted ambient pixels | `DirectorAmbientPixels.cpp` scales RGB in software; sets driver brightness 255; default enabled | Actual presentation/boot/locator/Emergency trace comparison; full LVGL/display build and physical UI/timing validation | Next ordinary-output candidate; no conversion in this batch |
| Audio GPIO22, programmable show line | `AudioPixelEngine.cpp` uses Adafruit as a GRB buffer and sends `getPixels()` through `AudioPixelOutput`; minimum-ten/saved-larger-count policy | Preserve GRB byte path, count/reallocation semantics and explicit write failure reporting; measure radio/MP3/I2S coexistence before changing transport | Retain native RMT path until dedicated design and parity evidence |
| Audio GPIO5, one fixed status pixel | `AudioStatusPixel.cpp` has a separate `AudioPixelOutput`; Emergency overrides faults, locate and tests | Independent status/show controllers, refresh/failure behavior, Emergency and live playback validation | Preserve fixed count one; evaluate separately from show line |
| Emergency Node GPIO2, programmable show line | `EstopPixelEngine.cpp` includes shared `ShowduinoPixelEngine.cpp`; shared code also serves deferred C3 Pixel | Count/rebind/segments/effects parity, default-ten/larger saved counts, white priority through ownership/blackout, immediate clear, full C3 build/radio validation | Separate later change; do not change shared default and accidentally convert deferred node |
| P4 GPIO23, programmable show line | Current baseline BoardConfig defaults `SHOWDUINO_SHOW_PIXEL_ENABLED` to 1 | Verify current native output implementation and GPIO24 resource ownership; preserve enabled boot behavior | Last migration candidate; PR #31's disabled default must not replace working baseline |
| P4 GPIO24, dedicated Emergency/signage | Separate dedicated native RMT line | Preserve its peripheral resources and highest-priority operation during any GPIO23 work | Retain native output; outside ordinary show-output migration |
| MOSFET four fitted identifier pixels | Existing `MosfetIdentifierPixels.cpp`, firmware 0.1.3 | None requested: owner says LED system is fine | Explicitly excluded from migration; unchanged |
| Standalone C3 LED-only | Owner explicitly deferred | No commissioning or build required for this stage | Deferred |

Adafruit-style method names alone do not prove compatible transport. The working Audio implementation does not use `Adafruit_NeoPixel::show()` to send frames; replacing only its strip type with the current shared adapter would also require redesigning the `getPixels()`/RMT boundary. The adapter currently has no `getPixels()` compatibility interface. Do not apply PR #31's older Audio files over this MP3/playback baseline.

## Complete reference builds

Existing checks cover P4/Director (PR #51) and S3 Lamp legacy/FastLED (PR #53). This batch adds complete unchanged Audio, Emergency, Comms and MOSFET builds. MOSFET compilation is evidence only and does not change its LED driver. No additional FastLED defaults are enabled.

Arduino CLI 1.1.1 and ESP32 core 3.3.12 are reference tool versions. Libraries are pinned in the workflow; Audio retains ESP8266Audio 2.4.1 from its existing source documentation. Audio's conservative 4 MB/no-PSRAM/min_spiffs/40 MHz profile and Comms' documented 8 MB/OPI/default_8MB profile come from their BoardConfig/README. Emergency and MOSFET generic 4 MB profiles are compile references, not confirmed installed settings. Do not infer physical board memory or boot mode from a green build.

The workflow retains logs, dependency versions, build options and partition CSV only. No binaries are published as releases and no upload step exists. Application size and partition evidence support future OTA planning but cannot establish runtime heap/stack use, inactive-slot installation, boot rollback, credentials or physical recovery.

## Remaining gates

1. Complete and retain reference build results; classify any failure as a build/profile/dependency finding rather than silently changing working firmware.
2. Keep Lamp hardware acceptance pending. Prepare the Director ambient candidate with actual-renderer parity separately; never bundle all-node driver replacements.
3. Design Audio transport adaptation only after comparing independent status/show outputs and playback timing. Carry measured flash/partition limits into OTA planning.
4. Handle Emergency Node and P4 outputs in their own reviewed batches, after the ordinary-output comparison. No safety protocol change belongs in these driver batches.
5. Require the owner's normal 22-second all-node show, assertion/clear/no-resume behavior and rollback evidence before accepting any physical conversion.

No merge, flash or claim of physical acceptance is made by this software preparation.
