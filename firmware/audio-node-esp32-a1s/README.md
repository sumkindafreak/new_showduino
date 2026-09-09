# Showduino Audio Node — ESP32-A1S / ES8388

```text
Status: IMPLEMENTED / HARDWARE TEST REQUIRED
Role: First production specialist Node (programme audio)
Firmware: 0.4.2
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

Arduino-ESP32 3.3.x. Libraries: core `WiFi`, `esp_now`, `SD`, `SPI`, `Wire`, `ESP_I2S`, `WebServer`, `Preferences`, plus **Adafruit NeoPixel**. WAV only. MP3 is PLANNED. SoftAP SSID `Showduino-Audio-XXXX` stays on ESP-NOW channel 1 at **http://192.168.5.1**. ESP-NOW keeps running while the AP is up.

## Test WAV

Create `/showduino/audio/system-test.wav` as **16-bit PCM WAV**, mono or stereo, **44.1 kHz or 48 kHz**. Also accepted: 8 / 16 / 22.05 / 32 kHz PCM.

## Bench

See [`docs/audio-node.md`](../../docs/audio-node.md) and [`docs/standalone-node-architecture.md`](../../docs/standalone-node-architecture.md). The firmware source and this README are authoritative for the GPIO22 external status-pixel and the 0.4.0 standalone SoftAP.

Do not flash from this document unless you intend a hardware bring-up session.
