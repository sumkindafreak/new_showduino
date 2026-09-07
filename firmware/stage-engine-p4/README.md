# Showduino Show Engine — Stage Controller (ESP32-P4)

```text
Status: ACTIVE
Role: Showduino Show Engine
Product: Stage Controller (ESP32-P4)
```

Canonical active Show Engine firmware:

```text
Director --ESP-NOW--> ESP32-S3 Comms Controller --UART--> this Show Engine
Node     --ESP-NOW--> ESP32-S3 Comms Controller --UART--> this Show Engine
```

> The Show Engine decides.

## Current implemented foundation

- Authoritative show/emergency runtime.
- Persistent P4 SD production discovery and transactional TEST/LOG timeline loading.
- Start/pause/resume/stop timeline execution independent of Director/browser presence.
- Dedicated S3 Comms UART on P4 GPIO4/5.
- P4 onboard ES8311 system/safety audio.
- Audio Node command routing and confirmed lifecycle tracking.
- GPIO24 emergency/designated-signage NeoPixel engine.
- GPIO23 local segmented theatrical Show Pixel Engine with shared 25-FX vocabulary.
- Plug-in Bus, storage, diagnostics and optional network foundations.

Persistent production format v1 still accepts only TEST/LOG cues. Production-file `AUDIO` and `PIXEL` cue types are not yet implemented.

## Arduino build / flash

The bench Stage Controller reports **16 MB** SPI flash. Do not flash a 32 MB image.

Arduino IDE:

- Board: **ESP32P4 Dev Module**
- Flash Size: **16MB (128Mb)**
- PSRAM: **Enabled**
- Partition Scheme: **Default**
- Chip Variant: **v3.00 or newer** if ROM banner is `ESP-ROM:esp32p4-eco2-...`

arduino-cli:

```text
arduino-cli compile --fqbn "esp32:esp32:esp32p4:PSRAM=enabled,FlashSize=16M,ChipVariant=postv3" firmware/stage-engine-p4/ShowduinoStageEngineP4
arduino-cli upload -p COMx --fqbn "esp32:esp32:esp32p4:PSRAM=enabled,FlashSize=16M,ChipVariant=postv3" firmware/stage-engine-p4/ShowduinoStageEngineP4
```

## P4 SD layout

```text
/showduino/productions/   authoritative runtime productions
/showduino/config/        persistent settings
/showduino/audio/system/  P4 system/safety WAVs
/showduino/logs/          bounded event logs
/showduino/diagnostics/   RUN:TEST exports
/showduino/backups/       config snapshots
/showduino/system/        storage metadata
```

## Comms UART

```text
P4 GPIO4 RX  <-  S3 GPIO17 TX
P4 GPIO5 TX  ->  S3 GPIO18 RX
115200 8N1
```

Onboard C6 remains unused/reserved: GPIO6, GPIO14-19, GPIO54.

## Emergency input

```text
GPIO25 → momentary pushbutton → GND
INPUT_PULLUP
```

Press latches emergency. Release does not clear. USB `EMERGENCY:CLEAR` is the maintenance path and refuses a still-held input. Director clearance retains the dual-action physical-hold/confirmation policy.

## Pixel outputs

### GPIO24 — emergency/designated-signage line

```text
configured count: 100
sign bundle size: 10 pixels
```

Normal state is automatically generated:

```text
pixel 0 GREEN, 1-9 OFF
pixel 10 GREEN, 11-19 OFF
pixel 20 GREEN, 21-29 OFF
...
```

Emergency changes **all GPIO24 pixels to bright white in one frame**.

### GPIO23 — local Show Pixel Line

Implemented non-blocking segmented FX engine:

```text
default count: 100
segment slots: 16
frame service: 20 ms
```

Shared effects in `protocol/showduino_pixel_fx.h`:

```text
OFF, SOLID, FADE_IN, FADE_OUT, PULSE, BREATHE, FLICKER, CANDLE, FIRE,
LIGHTNING, STROBE, RANDOM_STROBE, CHASE, BOUNCE, COMET, WIPE,
REVERSE_WIPE, BUILD, SPARKLE, TWINKLE, GLITCH, WARNING, PORTAL,
RAINBOW, CUSTOM_SEQUENCE
```

Hard safety rule:

> **EMERGENCY = ALL PIXELS BRIGHT WHITE.**

The emergency override sits above every GPIO23 segment/effect. On clear, GPIO24 returns to green locator markers and GPIO23 remains blacked out; interrupted effects never auto-resume.

### Pixel wiring standard

Each P4 data output uses a **470 Ω series resistor** near the controller/logic buffer:

```text
GPIO23 / 5V buffer → 470 Ω → Show Pixel DIN
GPIO24 / 5V buffer → 470 Ω → Signage Pixel DIN
```

Common ground is mandatory. A 74AHCT125/74HCT125-class 5 V logic buffer is recommended for final/long-cable installs. Use external 5 V pixel power, bulk capacitance and suitable power injection.

## Local USB / command console

USB is an additional maintenance input; it does not replace the Director → Comms → P4 path.

Core commands:

```text
HELP
STATUS:REQUEST
SHOW:START
SHOW:STOP
SHOW:PAUSE
SHOW:RESUME
PRODUCTION:LIST
PRODUCTION:LOAD:<id>
PRODUCTION:UNLOAD
PRODUCTION:STATUS
EMERGENCY:STOP
EMERGENCY:CLEAR
STORAGE:STATUS
AUDIO:STATUS
AUDIO:NODE:PLAY:<path>
RUN:TEST
```

P4 local pixel commands:

```text
PIXEL:STATUS
PIXEL:TEST
PIXEL:TEST:STOP
PIXEL:OFF
PIXEL:BLACKOUT
PIXEL:SOLID:<r>:<g>:<b>
PIXEL:BRIGHTNESS:<0-255>
PIXEL:SEGMENT:<id>:RANGE:<start>:<count>
PIXEL:SEGMENT:<id>:FX:<name>
PIXEL:SEGMENT:<id>:COLOR:<r>:<g>:<b>
PIXEL:SEGMENT:<id>:COLOR2:<r>:<g>:<b>
PIXEL:SEGMENT:<id>:BRIGHTNESS:<0-255>
PIXEL:SEGMENT:<id>:SPEED:<1-100>
PIXEL:SEGMENT:<id>:INTENSITY:<0-100>
PIXEL:SEGMENT:<id>:RANDOMNESS:<0-100>
PIXEL:SEGMENT:<id>:DURATION:<ms>
PIXEL:SEGMENT:<id>:REVERSE:<0|1>
PIXEL:SEGMENT:<id>:START
PIXEL:SEGMENT:<id>:STOP
PIXEL:SEGMENT:<id>:STATUS
```

Example simultaneous segments:

```text
PIXEL:SEGMENT:0:RANGE:0:8
PIXEL:SEGMENT:0:FX:LIGHTNING
PIXEL:SEGMENT:0:COLOR:255:255:255
PIXEL:SEGMENT:0:START

PIXEL:SEGMENT:1:RANGE:8:20
PIXEL:SEGMENT:1:FX:FIRE
PIXEL:SEGMENT:1:START
```

## Showduino Plug-in Bus

```text
SDA = GPIO7
SCL = GPIO8
100 kHz
```

## Current development order

Immediate platform work: **bench commission the P4 pixel lines**.

Specialist node order after current P4/Audio work:

```text
Audio Node → C3 Lantern Node → C3 Pixel Node → MOSFET Node
```

The previous Relay Node product direction is superseded. DMX remains parked until explicitly reopened.

See:

- [`docs/audio-pixel-engine.md`](../../docs/audio-pixel-engine.md)
- [`docs/hardware-pinout.md`](../../docs/hardware-pinout.md)
- [`docs/node-roadmap.md`](../../docs/node-roadmap.md)
- [`docs/production-storage.md`](../../docs/production-storage.md)
