# S3 Lamp optional FastLED evaluation

The standalone C3 LED-only node remains deferred. MOSFET LEDs remain excluded. This batch evaluates the active S3 Lamp's fitted seven-pixel GRB Jewel on GPIO8, without changing its renderer, effects, state machine, audio, protocol, wiring or saved configuration.

`SHOWDUINO_LAMP_USE_FASTLED` defaults to `0`: the existing direct Adafruit driver and firmware identity 0.4.3 remain selected. An explicit build flag of `1` selects the shared FastLED adapter and identity `0.4.4-fastled-dev1`. Other values fail compilation. This flag is specific to the S3 Lamp; other firmware is unaffected. No runtime toggle, saved backend setting or new user control is added.

The renderer already scales RGB according to its saved 0–100 brightness and never changes the strip's driver brightness. Both drivers therefore remain at full driver brightness. This avoids adopting the shared adapter's differing brightness-rescaling semantics. The fitted physical count remains seven; programmable-line default-ten rules do not change fitted Jewel geometry.

The opt-in path checks allocation and initialization, disposes failed instances and leaves readiness false. Subsequent initialization can retry. The default Adafruit initialization path retains its existing behavior. Emergency white still comes from the same unmodified renderer and ignores show brightness; normal solid/effect/identify/test requests retain their existing Emergency guards. Clear retains black output, stopped audio and no automatic resume.

## Evidence and limits

Host tests include the actual LampEngine implementation with time, config, audio and LED doubles. They compare 918 samples of state, visual, audio role and seven RGB values against both the candidate legacy and FastLED drivers and the unmodified PR #52 engine at commit `9e9600c9269758ed531e2f98878cf8cc4adb0326`. Cases cover every existing effect ID, brightness 0/1/37/100, ignition/living flame, blow stress, identify, jewel tests and Emergency during three interactive modes, including white at zero saved brightness and black/no-audio after clear. These traces are synthetic and are not the owner's physical 22-second show.

CI also runs existing Lamp node/carbide/motion tests and compiles the complete S3 Lamp sketch in both modes with core 3.3.12, Adafruit NeoPixel 1.15.5 and FastLED 3.10.8. The reference build profile is ESP32-S3, 8 MB flash, PSRAM disabled, default USB/CDC; it is not claimed to reproduce the installed binary's unrecorded settings. Logs are evidence, not flashable releases.

Host doubles and successful compilation cannot prove GRB ordering on the fitted Jewel, waveform/perceptual parity, electrical behavior, RF coexistence, live Audio FX timing or failure recovery. The pre-existing renderer API and its callers are not comprehensively hardened by this driver evaluation.

## Physical validation gate — not performed

1. Keep the preserved original source and known working upload method. Record exact board/core/partition/library settings and rollback image before touching hardware. Review the candidate diff and choose a spare or a recoverable test window.
2. Confirm legacy candidate boot/blackout, all seven positions, saved brightness, standalone ignition/flame/blow and Audio FX behavior. Run the owner's normal 22-second full-node show and Emergency/clear reference.
3. Build the explicit FastLED candidate with the reviewed board settings. Compare red/green/blue/white positions, low/full brightness, identify and jewel test, ignition/living/extinguish behavior and repeat boot.
4. During normal show, identify and jewel test, assert Emergency with saved brightness zero; all seven must become full white. Repeated normal/effect/blackout commands through the supported protocol must not displace Emergency. Clear must return black with audio stopped and require a fresh start.
5. Exercise sustained ESP-NOW traffic, ownership transitions, Wi-Fi/web activity and Audio FX together; check visible jitter, missed messages, resets and recovery. Compare the same show with the legacy candidate. Record results and rollback if any regression appears.

No merge or hardware upload is authorized by this document. FastLED remains an implemented opt-in candidate, not bench-tested or accepted. All other node drivers, P4 signage, production assets and the owner's original baseline remain untouched.
