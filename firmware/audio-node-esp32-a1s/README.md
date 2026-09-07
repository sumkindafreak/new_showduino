# Showduino Audio Node — ESP32-A1S / ES8388

```text
Status: IMPLEMENTED / HARDWARE TEST REQUIRED
Role: First production specialist Node (programme audio)
Firmware: 0.3.0
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
        ▼
local microSD → ES8388 DAC → speaker / headphone / line
onboard MIC1/MIC2 → ES8388 ADC → SOUND events to P4 (logical input only)
```

This Node plays theatrical / programme audio only. P4 local ES8311 remains system / safety audio. Do not send `AUDIO:LOCAL:` here.

## Hardware

**Ai-Thinker ESP32-Audio-Kit V2.2 A161** with **ESP32-A1S + ES8388**.

Not the older AC101 A1S (often silkscreen 2379). Pins are in `ShowduinoAudioNode/BoardConfig.h`. Factory observation on the development unit: classic ESP32 rev 3, 4 MB flash, DOUT 40 MHz, Ai-Thinker firmware v1.1.0. Runtime MAC discovery is authoritative.

## Sketch

```text
firmware/audio-node-esp32-a1s/ShowduinoAudioNode/
```

Arduino FQBN (conservative; match factory flash; do not assume PSRAM).
Arduino-ESP32 3.3.11 has no `dout` option — use `dio`:

```text
esp32:esp32:esp32:PSRAM=disabled,FlashSize=4M,PartitionScheme=min_spiffs,FlashMode=dio,FlashFreq=40
```

Arduino-ESP32 3.3.x. Libraries: core `WiFi`, `esp_now`, `SD`, `SPI`, `Wire`, `ESP_I2S`. WAV only. MP3 is PLANNED.

## Test WAV

Create `/showduino/audio/system-test.wav` as **16-bit PCM WAV**, mono or stereo, **44.1 kHz or 48 kHz**. Also accepted: 8 / 16 / 22.05 / 32 kHz PCM.

## Bench

See [`docs/audio-node.md`](../../docs/audio-node.md) for the verified A161 pin map, KEY1–KEY6, LED language, emergency / comms-loss policy, and hardware tests A–P.

Do not flash from this document unless you intend a hardware bring-up session.
