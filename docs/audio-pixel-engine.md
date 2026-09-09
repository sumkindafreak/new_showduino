# Showduino Audio and Pixel Engine

The P4 onboard ES8311 speaker provides Showduino system and safety audio only. Attraction/programme audio is exclusively produced by specialist Audio Nodes.

## 1. Audio architecture

### A. Showduino system / safety audio — P4 onboard ES8311

The Waveshare ESP32-P4-Module-DEV-KIT includes:

```text
ES8311 codec @ I²C 0x18
NS4150B power amplifier (PA_Ctrl GPIO53, active HIGH)
MX1.25 8Ω / 2W speaker connector
onboard microphone (unused by system audio)
```

This path is **never** an attraction/programme player. There is no Audio Node → P4 speaker fallback.

```text
1. BOOT
2. EMERGENCY
3. BEEP
4. TONE
5. ERROR
6. ACCEPTED
7. COMPLETE
8. SHUTDOWN     RESERVED — not implemented
```

Canonical SD files (`/showduino/audio/system/`):

```text
boot.wav
emergency.wav
beep.wav
tone.wav
error.wav
accepted.wav
complete.wav
shutdown.wav     present-ok / unused
```

Known-good WAV: PCM, 16-bit, mono or stereo, 44.1 kHz or 48 kHz preferred, 32 kHz accepted for existing emergency assets. Headers are validated. Unsupported encodings are rejected. Playback is not claimed until codec + I2S + amplifier + a valid file + I2S bytes flowing are all true.

Verified I2S mapping (Waveshare wiki + ESP-IDF P4 examples):

```text
I2C SDA     GPIO7
I2C SCL     GPIO8
I2S DSDIN   GPIO9     P4 DOUT → ES8311 DAC
I2S LRCK    GPIO10    not a status LED
I2S ASDOUT  GPIO11    unused for playback
I2S SCLK    GPIO12
I2S MCLK    GPIO13
PA enable   GPIO53
```

The old external PCM5102A path (GPIO20/21/22) is retired and compiled out.

Priority (local P4 only):

```text
EMERGENCY
    ↓
ERROR
    ↓
other system notifications
```

Emergency always wins. Clearing emergency stops the WAV and returns IDLE (no resume). Emergency safety does not depend on `emergency.wav` playing.

### B. Show/programme audio — Audio Node (ESP32-A1S / ES8388)

Theatrical and production audio belongs to the first specialist **Audio Node**:

```text
music
voice / dialogue
ambience
scare SFX
stingers
timeline audio (future cue dispatch)
```

The Node stores WAV files on its own microSD and reports a confirmed lifecycle. P4 decides; Comms transports; the Node plays.

The retired P4 PCM5102A path is not a substitute for the Audio Node.

See [`docs/audio-node.md`](audio-node.md).

## 2. I2S ownership

The ESP32-P4 has one I2S peripheral. It is owned by onboard ES8311 system audio.

Attraction audio does not share this peripheral. The Audio Node has its own codec and SD.

## 3. Audio storage

Recommended P4 SD layout:

```text
/showduino/audio/system/
  boot.wav
  emergency.wav
  beep.wav
  tone.wav
  error.wav
  accepted.wav
  complete.wav
  shutdown.wav     present-ok / unused

/showduino/audio/show_machine/
  same filenames — used when the system/ copy is missing or not a valid engine WAV
```

Valid engine WAV: PCM or WAVE_FORMAT_EXTENSIBLE PCM, 16-bit, mono or stereo, 32 / 44.1 / 48 kHz. Missing or invalid system sounds must never stop the Show Engine booting.

## 4. Pixel architecture — implemented P4 foundation

Pixels are part of the Showduino show language, not decoration.

The P4 owns two physically separate local NeoPixel outputs:

```text
P4 GPIO24  → dedicated emergency/signage pixel line
P4 GPIO23  → one general-purpose theatrical Show Pixel Line
```

Additional theatrical pixel outputs belong on specialist Pixel Nodes rather than consuming more local P4 GPIOs.

The GPIO23 Show Pixel Engine is now implemented as a non-blocking, segmented FX engine. The direct `PIXEL:` maintenance/bench command path is live. Production format v1 still accepts only TEST/LOG cues, so production-file `PIXEL` cue parsing/routing remains a later integration milestone.

### Segment model

One physical strip may run multiple independent visual regions at once:

```text
pixels 0-7    → LIGHTNING
pixels 8-10   → SOLID BLUE
pixels 11-29  → FIRE
pixels 30-49  → SLOW RED PULSE
pixels 50-79  → WARM FLICKER
```

Default P4 configuration:

```text
SHOWDUINO_SHOW_PIXEL_PIN          23
SHOWDUINO_SHOW_PIXEL_COUNT        100
SHOWDUINO_SHOW_PIXEL_MAX_SEGMENTS 16
SHOWDUINO_SHOW_PIXEL_FRAME_MS     20
```

Count and segment limit are compile-time configuration values in `BoardConfig.h`.

Each segment owns:

```text
start
count
effect
primary colour
secondary colour
brightness
speed
intensity
randomness
direction/reverse
duration
runtime phase/state
```

No effect is allowed to use blocking `delay()` calls. The Show Engine continues servicing communications, safety, audio and timeline state while FX advance from `millis()`.

If configured segments overlap, the later segment slot is rendered later and therefore wins for overlapping pixels. Normal authoring should avoid accidental overlap unless that deterministic behaviour is specifically wanted.

## 5. Shared 25-effect Showduino FX vocabulary

The common effect IDs live in `protocol/showduino_pixel_fx.h` so the later C3 Pixel Node can use the same language as the P4.

```text
01 OFF / BLACKOUT
02 SOLID
03 FADE_IN
04 FADE_OUT
05 PULSE
06 BREATHE
07 FLICKER
08 CANDLE
09 FIRE
10 LIGHTNING
11 STROBE
12 RANDOM_STROBE
13 CHASE
14 BOUNCE
15 COMET
16 WIPE
17 REVERSE_WIPE
18 BUILD
19 SPARKLE
20 TWINKLE
21 GLITCH
22 WARNING / WARNING_RED
23 PORTAL / PORTAL_GLOW
24 RAINBOW
25 CUSTOM_SEQUENCE
```

Effects are parameterised rather than multiplied into colour-specific variants. `LIGHTNING`, for example, can be recoloured and adjusted for speed/intensity/randomness instead of creating separate RED_LIGHTNING, BLUE_LIGHTNING, etc.

## 6. P4 pixel bench command model

Implemented direct commands:

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

Example:

```text
PIXEL:SEGMENT:0:RANGE:0:8
PIXEL:SEGMENT:0:FX:LIGHTNING
PIXEL:SEGMENT:0:COLOR:255:255:255
PIXEL:SEGMENT:0:SPEED:70
PIXEL:SEGMENT:0:RANDOMNESS:90
PIXEL:SEGMENT:0:START

PIXEL:SEGMENT:1:RANGE:8:12
PIXEL:SEGMENT:1:FX:FIRE
PIXEL:SEGMENT:1:START
```

Both segments then run at the same time on the same GPIO23 chain.

`PIXEL:TEST` is non-blocking and performs red, green, blue, white and a one-pixel chase before returning to blackout.

## 7. Emergency/signage pixel line — GPIO24

GPIO24 is not an ordinary show lane. It is dedicated to illuminated designated emergency signage.

Physical convention:

```text
1 emergency exit sign = 10 NeoPixels
maximum configured line = 100 NeoPixels
therefore up to 10 sign bundles per line
```

Normal state is generated automatically by the P4 safety layer:

```text
Sign 1  pixels 0-9:    pixel 0 GREEN, pixels 1-9 OFF
Sign 2  pixels 10-19:  pixel 10 GREEN, pixels 11-19 OFF
Sign 3  pixels 20-29:  pixel 20 GREEN, pixels 21-29 OFF
...
```

For 100 pixels the pattern is conceptually:

```text
G.........G.........G.........G.........G.........G.........G.........G.........G.........G.........
```

The production/timeline does not own this pattern.

## 8. Global emergency pixel rule — hard safety policy

**EMERGENCY = ALL PIXELS BRIGHT WHITE.**

This rule applies above every normal FX, segment, colour and node role:

```text
P4 GPIO24 emergency/signage line → every pixel WHITE
P4 GPIO23 Show Pixel Line         → every pixel WHITE
C3 Pixel Nodes                    → every pixel WHITE (when implemented)
Lantern/pixel-capable nodes       → every pixel WHITE where applicable
future pixel-capable nodes        → every pixel WHITE
```

For GPIO24, all sign groups are prepared in memory and transmitted as one frame so they change together rather than sign-by-sign.

For GPIO23, the emergency override bypasses every segment and FX and writes full-line white. The emergency frame is periodically refreshed while the latch is active.

On authorised emergency clear:

```text
GPIO24 signage → returns to automatic GREEN locator pattern
GPIO23 show pixels → BLACKOUT / safe idle
interrupted show FX → DO NOT auto-resume
```

No normal production command is allowed to override emergency white.

## 9. Pixel data-line electrical standard

For each P4 NeoPixel data output, Showduino standardises on a **470 Ω series resistor**:

```text
GPIO23 / logic buffer ── 470 Ω ──> Main Show Pixel DIN
GPIO24 / logic buffer ── 470 Ω ──> Emergency/Signage Pixel DIN
```

Place the resistor close to the P4-side driver/logic buffer.

The resistor does **not** convert 3.3 V logic to 5 V. Short bench wiring may work directly from a P4 GPIO, but final installations and longer cables should use a 5 V-compatible logic buffer such as a 74AHCT125/74HCT125-class device:

```text
P4 GPIO → 5 V logic buffer → 470 Ω → NeoPixel DIN
```

Also required/recommended:

- P4 ground and pixel power-supply ground must be common.
- Power pixels from a suitably sized external 5 V supply rather than through the P4.
- Add roughly 1000 µF bulk capacitance across 5 V/GND near the start of a substantial pixel line.
- Power injection should be designed for the actual pixel count/current and cable length.

## 10. Timeline pixel direction

The local engine is ready to receive show intent, but persistent production format v1 still needs a real `PIXEL` cue type added before this becomes authoritative production-file behaviour.

Desired production cue shape:

```json
{
  "id": "lightning_1",
  "timeMs": 2500,
  "type": "PIXEL",
  "target": "p4.pixels.segment0",
  "action": "FX",
  "value": "LIGHTNING"
}
```

The timeline should issue high-level effect intent. It must not schedule thousands of individual LED updates.

Local P4 path will therefore be:

```text
P4 Timeline → P4 Pixel Engine → GPIO23
```

No Comms S3 or ESP-NOW hop is required for the local line.

## 11. Example future audio + pixel show

```text
00:00  Audio Node ambience LOOP
00:02  P4 segment 2 warm glow
00:05  P4 segment 0 LIGHTNING
00:05  Audio Node thunder PLAY
00:08  P4 pixels FADE_OUT
00:10  Audio fade / stop
        SHOW COMPLETE
```

This becomes a true production only after AUDIO/PIXEL production cue types are accepted and dispatched by the P4 production parser.

## 12. Hardware split decision

```text
P4 Show Engine
├── onboard ES8311 → Showduino system/safety sounds only
├── GPIO24 emergency/signage line → grouped 10-pixel exit signs
├── GPIO23 segmented Show Pixel Line → 25 shared FX
└── authoritative routing/state for specialist nodes

Specialist Nodes
├── Audio Node → attraction/programme audio
├── C3 Lamp Node → specialist-node hardware after Audio; firmware present, hardware test required
├── C3 Pixel Node → shared Showduino FX vocabulary
└── MOSFET Node → planned replacement for the old Relay Node concept
```

**Audio Node — IMPLEMENTED / HARDWARE TEST REQUIRED.**  
**P4 local pixel engine — IMPLEMENTED / HARDWARE BENCH TEST REQUIRED.**  
**Production PIXEL cue parsing — NOT YET IMPLEMENTED.**
