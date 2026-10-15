# Showduino Audio Node — ESP32-A1S / ES8388

```text
Status: IMPLEMENTED / HARDWARE TEST REQUIRED
Role: First production specialist Node (programme audio)
Firmware: 0.4.7
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
        ├── GPIO5 → one local WS2812 / NeoPixel status indicator
        └── GPIO22 → programmable Show Pixel Line (10 pixels by default)
```

This Node plays theatrical / programme audio only. P4 local ES8311 remains system / safety audio. Do not send `AUDIO:LOCAL:` here.

The single GPIO5 status pixel is **local diagnostics/connectivity only** and is not authored as a show output. GPIO22 is a separate programmable Show Pixel Line, defaulting to 10 pixels unless a valid commissioned/saved count overrides it. Emergency white overrides the entire configured GPIO22 line.

## Hardware

**Ai-Thinker ESP32-Audio-Kit V2.2 A161** with **ESP32-A1S + ES8388**.

Not the older AC101 A1S (often silkscreen 2379). Pins are in `ShowduinoAudioNode/BoardConfig.h`. Factory observation on the development unit: classic ESP32 rev 3, 4 MB flash, DOUT 40 MHz, Ai-Thinker firmware v1.1.0. Runtime MAC discovery is authoritative.

### External status pixel

Connect one standard 800 kHz WS2812 / NeoPixel:

```text
Audio Node GPIO5  ── 330 Ω ── DIN  WS2812 (single local status LED)
Audio Node GND   ───────────── GND
5 V supply       ───────────── 5V
```

For the **separate GPIO22 show line**, connect GPIO22 through its own 330 Ω series resistor to the first programmable WS2812 DIN, with common ground and an adequately rated external 5 V supply. Do not connect the GPIO5 status LED and GPIO22 show pixels to the same data pin.

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

`PIXEL:TEST` commissions the GPIO22 show pixel line. The GPIO5 status indicator is independent and remains dedicated to local status.

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

See [`docs/audio-node.md`](../../docs/audio-node.md) and [`docs/standalone-node-architecture.md`](../../docs/standalone-node-architecture.md). The firmware `BoardConfig.h` is authoritative for the GPIO5 single status pixel, GPIO22 programmable show line and current SoftAP implementation.

Do not flash from this document unless you intend a hardware bring-up session.


Pixel indication follows [`docs/status-colour-standard.md`](../../docs/status-colour-standard.md). In the checked-in hardware definition the programmable Audio pixel line is GPIO22; Emergency forces that configured line to full 255 white and Locate uses moving white.
