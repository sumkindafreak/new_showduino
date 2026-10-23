# Showduino Audio Node — ESP32-A1S / ES8388

```text
Status: IMPLEMENTED / HARDWARE TEST REQUIRED
Role: First production specialist Node (programme audio)
Firmware: 0.4.3
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
        └── GPIO22 → configurable WS2812 / NeoPixel theatrical line (1–512 pixels)
```

This Node plays theatrical / programme audio only. P4 local ES8311 remains system / safety audio. Do not send `AUDIO:LOCAL:` here.

GPIO22 is a **programmable Showduino Pixel Line on the existing Audio Node identity**, not a separate status-only LED or a second Pixel Node. It supports configured count, segments and theatrical FX. Current `main` firmware does not provide an independent GPIO5 status pixel; the older status-colour scheme below does not apply to this build.

## Hardware

**Ai-Thinker ESP32-Audio-Kit V2.2 A161** with **ESP32-A1S + ES8388**.

Not the older AC101 A1S (often silkscreen 2379). Pins are in `ShowduinoAudioNode/BoardConfig.h`. Factory observation on the development unit: classic ESP32 rev 3, 4 MB flash, DOUT 40 MHz, Ai-Thinker firmware v1.1.0. Runtime MAC discovery is authoritative.

### GPIO22 theatrical pixel line

Connect the first pixel in a standard 800 kHz WS2812 / NeoPixel chain:

```text
Audio Node GPIO22 ── 330 Ω ── DIN  first WS2812
Audio Node GND   ───────────── GND
5 V supply       ───────────── 5V
```

Use a common ground between the Audio Node and a correctly rated, protected external 5 V pixel supply. Verify data-level compatibility with the actual strip; add a 3.3 V → 5 V logic-level buffer where needed, particularly with long/noisy wiring. Fit suitable bulk capacitance and power distribution for the installed pixel count.

The old status-colour table applied to the superseded status-only design. On current `main`, this line displays authored effects, commissioning tests, Locate and Emergency white override; it is **not** a live Audio/ESP-NOW state indicator.

**First-time commissioning for the fixed 10-pixel acceptance strip:** the engine reads the saved `pix` count with a fallback of **0**, so an unconfigured line stays dark even though `BoardConfig.h` contains a count macro of 10. With the node safely powered and accessible via its local console/WebUI or an authorised routed command, send:

```text
PIXEL:COUNT:10
PIXEL:INIT
PIXEL:STATUS
PIXEL:TEST
```

Check for `PIXEL:COUNT:OK:10`, `PIXEL:INIT:OK:10`, and `PIXEL:STATUS:READY` with configured/active count 10. The count is saved and applied on later boots. `PIXEL:TEST` shows red, green, blue and white (700 ms each), followed by a moving white chase; at 10 pixels the sequence is approximately **3.1 seconds**, not 1.5 seconds. `LED:TEST` remains a legacy alias. During Emergency, all **configured** pixels are forced white; clearing leaves the line black until a new command.

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

See [`docs/audio-node.md`](../../docs/audio-node.md) and [`docs/standalone-node-architecture.md`](../../docs/standalone-node-architecture.md). The checked-in firmware and this README describe the GPIO22 theatrical pixel line in `main` 0.4.3; verify the version actually installed before commissioning.

Do not flash from this document unless you intend a hardware bring-up session.


General status-colour guidance is in [`docs/status-colour-standard.md`](../../docs/status-colour-standard.md), but it must not be mistaken for an independent Audio Node status LED on current `main`. The configured GPIO22 line is overridden to full white on Emergency and uses moving white during Locate.
