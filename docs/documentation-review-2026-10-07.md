# Showduino documentation review — 7 October 2026

Reviewed source: `main` at `526af84cc04d96b3f4d9a51bc3a0ef1470da0b69`. This change includes the requested P4 Emergency ambience policy update in addition to documentation corrections. Manual revision 0.3-RC describes that final change; the SHA identifies the source reviewed before this patch, not an accepted set of installed binaries.

## Component inventory

| Component | Reviewed source | Updated in this change | Source |
| --- | --- | --- | --- |
| Product | 1.0.0-rc.1 | unchanged | `protocol/showduino_version.h` |
| P4 Stage Engine | 0.6.8 | 0.6.9 | P4 `BoardConfig.h` |
| Comms S3 | 0.5.4 | unchanged | Comms `BoardConfig.h` |
| Director | 0.9.11-director | unchanged | Director `src/StorageConfig.h` |
| Audio Node | 0.4.3 | unchanged | Audio `BoardConfig.h` |
| S3 Lamp Node | 0.4.3 | unchanged | Lamp `BoardConfig.h` |
| C3 Pixel Node | 0.1.1 | unchanged | Pixel `BoardConfig.h` |
| C3 Emergency Node | 0.3.1 | unchanged | Emergency `BoardConfig.h` |
| MOSFET Node | 0.1.3 | unchanged | MOSFET `BoardConfig.h` |

Source labels do not prove flashed firmware or physical acceptance. Existing release manifests remain records of their original release, not new accepted binary inventories.

## Corrections and evidence

| Outdated/missing claim | Corrected documentation | Code evidence |
| --- | --- | --- |
| MOSFET still planned | Active four-channel RC powered-output firmware, Director control, SHDO deploy and identifier pixels; physical acceptance open | `firmware/mosfet-node-esp32/ShowduinoMosfetNode/`, P4 `src/nodes/MosfetNodeLink.cpp`, `protocol/showduino_shdo.h` |
| Wireless station uses NC input | GPIO4 momentary active-LOW pushbutton, OLED and same-peer GPIO2 Pixel Line; intended button pin still unverified | C3 Emergency `BoardConfig.h` |
| Persistent storage only TEST/LOG or omits MOSFET | Bounded TEST/LOG/PIXEL/AUDIO/LAMP/MOSFET; ESTOP, IO and AMBIENCE cue types still absent | P4 `src/ProductionFormat.cpp` |
| Browser RAM timeline implies MOSFET support | Browser RAM envelope accepts PIXEL/AUDIO:NODE only; powered-output SHDO persistence is a separate path | `protocol/showduino_web_timeline_upload_policy.h`, P4 `src/WebApiHandler.cpp` |
| Two audio paths only | ES8311 system audio, independent P4 PCM5102A ambience, Audio Node programme audio | P4 `BoardConfig.h`, `src/StageAmbience.cpp` |
| Generic I/O missing from user guides | GPIO46/47 configurable digital lines, SD persistence, debounce/pulse and safe boot/stop; service commands only | P4 `src/ShowduinoIO.cpp/.h` and `.ino` |
| Red/blue Locate / generic green-ready indicators | Flashing white Locate, solid full-white Emergency, harmonised semantic indicators | Director `DirectorAmbientPixels.cpp`, `docs/status-colour-standard.md` and node indicator engines |
| Lamp Fermion/old component labels | Adafruit Audio FX UART WAV roles; current source inventory | Lamp `BoardConfig.h`, `src/LampAudio.cpp`, Director `src/StorageConfig.h` |

## Requested Emergency audio policy

All normal programme/prop/ambience playback stops on Emergency. The Audio Node remains muted, the Lamp switches to its local emergency WAV, and PCM5102A switches to a looping `/showduino/audio/system/emergency.wav`, falling back to `/showduino/audio/show_machine/emergency.wav`. ES8311 continues its dedicated emergency announcement. PCM5102A uses 100% software volume for that announcement; physical amplifier gain must still be commissioned.

Repeated assertions do not restart the PCM5102A announcement. Normal ambience commands cannot replace, mute or stop it while latched. Authorised clear stops the announcement and leaves ambience idle. Normal show STOP also stops background ambience when Emergency is clear. Missing/invalid WAV, SD or I2S failure leaves normal playback stopped and never clears Emergency.

## Remaining limits

- Generic I/O and ambience have no dedicated Director/Studio editor or persistent cue type; input scene binding is not implemented.
- Emergency-node pixel SHDO compilation emits ESTOP cues, but persistent format-v1 does not accept that dedicated cue type.
- PCM5102A mono mapping and partial nonblocking I2S writes remain playback limitations. Stereo PCM is the initial test baseline; actual continuity and concurrent ES8311 playback need hardware testing.
- MOSFET pin/identifier and Emergency button verification flags remain unverified in source. This pass does not manufacture bench sign-off.
- Historical architecture decisions and dated release/acceptance records are retained as history.

See the [manual gaps](manual/SHOWDUINO_MANUAL_GAPS.md), especially GAP-022 and GAP-023, and the [pre-opening checklist](manual/SHOWDUINO_PRE_OPENING_CHECKLIST.md). Host regression tests compile the real ambience player with simulated dependencies; they do not prove DAC output, radio delivery or a complete current hardware firmware build.
