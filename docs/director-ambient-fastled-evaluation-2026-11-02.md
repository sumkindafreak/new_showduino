# Director ambient FastLED evaluation

This isolated candidate targets only the two fitted GRB ambient pixels on GPIO17. GPIO18 remains reserved for GT911 interrupt. `SHOWDUINO_DIRECTOR_AMBIENT_USE_FASTLED` defaults to 0, retaining the direct Adafruit driver. An explicit value 1 selects shared FastLED, with a serial startup message identifying the evaluation backend. Other values fail compilation. No user setting or NVS schema changes.

The existing renderer already scales RGB in software with the driver at brightness 255. Its ordinary palette, fades, presentation hold, state mapping, pulses, locator sequence, brightness clamp and frame cadence are retained. FastLED initialization checks allocation/binding and keeps readiness false on failure. The default backend retains its current initialization behavior.

## Deliberate Emergency-priority correction

The original `directorAmbientLoop` rendered locator frames ahead of Emergency and accepted new locator starts during Emergency. Its locator OFF phases could therefore replace Emergency white. The candidate cancels an active locator when entering Emergency, refuses new locator starts in that mode and prevents locator rendering from taking precedence. This correction applies to both backends and is distinct from the driver selection. It changes ambient indication priority only, not the Director's Emergency clear protocol or show/playback state.

The actual original renderer at commit `b103d6f7cdcb2e4ce166fd43d7c6266cbb08e97c` reproduces a failing Emergency-white assertion in the host test. Both candidate backends pass with active/new locator requests, cosmetics disabled and requested brightness zero (the existing brightness setter clamps zero to one; Emergency bypasses scaling).

Ordinary actual-renderer traces match the original across 1,756 samples: modes, show/link states, brightness changes, boot/presentation hold, pulse events, degraded/synchronizing states and locator behavior outside Emergency. Host time/LED/UI doubles do not prove actual display, touch, radio, physical waveform or timing.

CI compiles the complete Director sketch with its real LVGL/display/touch dependencies in legacy and explicit FastLED modes. Core 3.3.12, LVGL 9.6.0, GFX Library for Arduino 1.6.9, TAMC_GT911 1.0.2, Adafruit NeoPixel 1.15.5 and FastLED 3.10.8 are pinned reference versions. The 16 MB/OPI/app3M_fat9M_16MB profile is a reference, not a claim to reproduce an installed binary. Existing Director tests also run.

## Physical gate — not performed

Review the diff and exact board/core/partition/library settings, preserve a known working rollback image, then compare the two-pixel RGB/GRB order, boot-to-ready handoff, display/touch responsiveness, state fades, pulses and locator under traffic. Assert Emergency during a locator OFF phase, with cosmetics disabled and lowest brightness: require steady full white, no later locator overwrite, and correct return to the authoritative state after clear. Repeat the 22-second show and Emergency/clear reference. No upload or merge is performed or authorized by this document.

Audio remains unchanged; its separate rendering/transport design is documented alongside this candidate. MOSFET LEDs remain excluded, C3 LED-only deferred, Lamp hardware acceptance pending, and P4 outputs untouched.
