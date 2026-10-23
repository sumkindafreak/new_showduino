# Showduino Audio Node — ESP32-A1S / ES8388

```text
Status: IMPLEMENTED / HARDWARE TEST REQUIRED
Role: First production specialist Node (programme audio)
Firmware: 0.5.0-mp3-rc1
```

```text
Director / WebUI
        │ request
        ▼
P4 Show Engine
        │ authoritative decision
        ▼
Comms S3
        │ ESP-NOW
        ▼
Audio Node (this firmware)
        │
        ├── local microSD → ES8388 DAC → speaker / headphone / line
        ├── onboard MIC1/MIC2 → ES8388 ADC → SOUND events to P4
        └── GPIO22 → one external WS2812 / NeoPixel status indicator
```

This Node plays theatrical / programme audio only. P4 local ES8311 remains system / safety audio. Do not send `AUDIO:LOCAL:` here.

The single GPIO22 status pixel is **local diagnostics/connectivity only**. It is not a programme pixel line and is never authored as part of a show.

## Hardware

**Ai-Thinker ESP32-Audio-Kit V2.2 A161** with **ESP32-A1S + ES8388**.

Not the older AC101 A1S (often silkscreen 2379). Pins are in `ShowduinoAudioNode/BoardConfig.h`. Factory observation on the development unit: classic ESP32 rev 3, 4 MB flash, DOUT 40 MHz, Ai-Thinker firmware v1.1.0. Runtime MAC discovery is authoritative.

### External status pixel

Connect one standard 800 kHz WS2812 / NeoPixel:

```text
Audio Node GPIO22 ── 330 Ω ── DIN  WS2812
Audio Node GND   ───────────── GND
5 V supply       ───────────── 5V
```

Use a common ground between the Audio Node and the pixel supply. For a single nearby pixel, the ESP32's 3.3 V data level is normally suitable; if the lead becomes long or noisy, add a 3.3 V → 5 V logic-level buffer. A small bulk capacitor across the pixel 5 V/GND is recommended when wiring it remotely.

Status language:

| Pixel | Meaning |
|---|---|
| Blue slow blink | Booting |
| Amber blink | Searching for P4 GRANT |
| Slow violet | Standalone (local SoftAP WebUI) |
| Green steady | P4-owned / idle |
| Bright-green kick | Fresh ESP-NOW traffic received |
| Cyan | Playing / looping |
| Cyan blink | Loading / stopping |
| Purple slow blink | Paused |
| Orange triple flash | No SD/storage |
| Red fast blink | Fault |
| Bright white | Emergency override |

`PIXEL:TEST` runs a 1.5 s RGBW commissioning pattern. `LED:TEST` remains as a legacy alias.

## Sketch

```text
firmware/audio-node-esp32-a1s/ShowduinoAudioNode/
```

Arduino FQBN (conservative; match factory flash; do not assume PSRAM).
Arduino-ESP32 3.3.11 has no `dout` option — use `dio`:

```text
esp32:esp32:esp32:PSRAM=disabled,FlashSize=4M,PartitionScheme=min_spiffs,FlashMode=dio,FlashFreq=40
```

Arduino-ESP32 3.3.x. Libraries: core `WiFi`, `esp_now`, `SD`, `SPI`, `Wire`, `ESP_I2S`, `WebServer`, `Preferences`, plus **Adafruit NeoPixel**. **WAV and MP3 (MP3 is experimental in this branch)**. SoftAP SSID `Showduino-Audio-XXXX` stays on ESP-NOW channel 1 at **http://192.168.5.1**. ESP-NOW keeps running while the AP is up.

## Experimental MP3 playback — 0.5.0-mp3-rc1

This feature branch is stacked on top of the draft 0.4.4 WAV stutter repair.
**Do not merge until the stutter repair and this MP3 decoder both pass hardware tests.**

### Arduino Library Manager dependency

Install **ESP8266Audio**, by Earle F. Philhower III, version **2.4.1**.
It includes the `AudioGeneratorMP3` software MP3 decoder and
`AudioFileSourceSD` file source. The library works on ESP32 without PSRAM,
but decoder heap consumption and decoding performance must be verified on
our actual 4 MB flash / no-PSRAM ESP32-A1S with ESP-NOW active.

Source: https://github.com/earlephilhower/ESP8266Audio
Licence: **GPL-3.0**. Review the source-distribution and licence
compatibility obligations before any Showduino commercial distribution.
No ESP8266Audio source code has been copied into the Showduino repository.

### SD card

Use standard, non-encrypted MPEG Layer III MP3 tracks (start with
constant-bitrate 128 kbps, 44.1 kHz stereo). Keep WAV for the most critical,
low-latency and precisely timed show cues.

Examples:

```text
/showduino/audio/music/ambience.mp3
/showduino/audio/dialogue/intro.mp3
/showduino/audio/effects/thunder.wav
/The_Chamber/ambience/theme.mp3
```

Existing `AUDIO:NODE:PLAY:<path>`, `AUDIO:NODE:LOOP:<path>`,
`AUDIO:NODE:STOP`, `PAUSE`, `RESUME`, `VOLUME`, fades/duck,
`OWN:GRANT`, emergency STOP/CLEAR, and file inventories apply to both
formats. The physical A1S codec and I2S GPIOs are unchanged.

The source dispatcher routes WAV into the native 16-bit PCM engine, MP3
into `AudioGeneratorMP3`, and decoded stereo PCM to a 4096-byte I2S
staging output. Only one track/output owns I2S at a time. No HTTP MP3
streaming or two-track mixing is enabled.

The `AUDIO:STATUS` positional numbers for MP3 represent **compressed
file bytes**, not playback milliseconds. User-interface elapsed time is
not yet calculated from MP3 frame timestamps.

### Test steps and release blockers

1. First run the 0.4.4 branch and confirm WAV no longer stutters.
2. Install ESP8266Audio 2.4.1, compile this branch with Arduino-ESP32
   3.3.11 and the documented A161 build board settings.
3. Put `music/ambience.mp3` on the SD card and use serial commands
   `AUDIO:PLAY:music/ambience.mp3` and `AUDIO:STATUS`.
4. Listen to 128/192/320 kbps CBR and VBR, mono and stereo, at 44.1/48kHz.
5. Exercise pause/resume, stop, loop, preemption, fades, volume/duck,
   SD removal, speaker/headphone/line and memory after 30 repeated starts.
6. With ESP-NOW, pixels and SoftAP enabled, play a complete long file
   without audible gaps and confirm that emergency interrupts MP3
   immediately and clears safely.
7. Compile and hardware tests must pass before merging this draft.
   Avoid using MP3 as the emergency sound until validated.

## Test WAV

Create `/showduino/audio/system-test.wav` as **16-bit PCM WAV**, mono or stereo, **44.1 kHz or 48 kHz**. Also accepted: 8 / 16 / 22.05 / 32 kHz PCM.

## First-pass stuttering repair (0.4.4)

The previous audio task slept 2 ms after every tiny 512-byte read/write.
That consumed most of the deadline for 44.1/48 kHz stereo PCM.
This release uses a 4 KiB PCM staging block, no forced per-block delay,
retries short I2S writes without dropping samples, and defers SD configuration
writes and card-size polling until audio is idle. No PSRAM is required;
Director protocol, Showduino ownership and Emergency paths are unchanged.

### Bench comparison before accepting this patch

1. Put the same 16-bit 44.1 or 48 kHz test WAV on the Audio Node SD card.
2. On 115200-baud serial, run `AUDIO:TEST` or `AUDIO:PLAY:<path>`.
3. During playback run `AUDIO:STATUS` and record `SD_READ_MAX_US`,
   `I2S_WRITE_MAX_US`, `short_writes`, `underruns` and `ERR`.
   Note: the existing `underruns` counter actually measures failed I2S writes,
   not hardware DMA underflows.
4. Listen with ESP-NOW, the configured pixel line and SoftAP in operation.
5. Re-test PAUSE/RESUME, STOP, LOOP, track end, bad WAV, card removal,
   standalone use, P4 ownership transitions and Emergency STOP/CLEAR.

Not hardware-verified. If audio still stutters, the next step is a
prefilled ring buffer with independent SD-reader and I2S-writer tasks,
including measurements of genuine starvation.

## Bench

See [`docs/audio-node.md`](../../docs/audio-node.md) and [`docs/standalone-node-architecture.md`](../../docs/standalone-node-architecture.md). The firmware source and this README are authoritative for the GPIO22 external status-pixel and the 0.4.0 standalone SoftAP.

Do not flash from this document unless you intend a hardware bring-up session.


Pixel indication follows [`docs/status-colour-standard.md`](../../docs/status-colour-standard.md). In the checked-in hardware definition the programmable Audio pixel line is GPIO22; Emergency forces that configured line to full 255 white and Locate uses moving white.
