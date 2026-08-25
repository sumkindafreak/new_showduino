# Showduino Show Engine — Stage Controller (ESP32-P4)

```text
Status: ACTIVE
Role: Showduino Show Engine
Product: Stage Controller
Board: Waveshare ESP32-P4-Module-DEV-KIT
```

Canonical active Show Engine firmware. Folder name `stage-engine-p4` is historical and may be renamed later.

## Target product path

```text
Director --ESP-NOW--> onboard ESP32-C6 --integrated transport--> this P4 Show Engine
Node     --ESP-NOW--> onboard ESP32-C6 --integrated transport--> this P4 Show Engine
Browser  --Wi-Fi----> onboard ESP32-C6 ----------------------> P4 Web services
```

## Firmware transition note

The current sketch still contains the previously working UART path for the **external C3 SuperMini**. That UART remains a compatibility/rollback path while the onboard C6 migration is developed.

The separate C3 is no longer part of the final Stage Controller hardware baseline.

## Constitution

> The Show Engine decides.

Owns:

- Authoritative show/runtime/emergency state
- Timeline/cues as implemented
- Project/runtime storage
- Safety policy
- Web UI / Web API / WebSocket
- Node command lifecycle
- Stage Controller local outputs
- System time

## Current hardware baseline

```text
ESP32-P4                     Show Engine
onboard ESP32-C6             Communications Engine target
P4 RTC + RTC battery header  timekeeping target
onboard ES8311 + NS4150B     Showduino/system audio
external PCM5102A            show/programme audio
onboard microSD              shows, WebUI, logs, media
Ethernet / USB               board-native I/O
GPIO24                       emergency NeoPixel
GPIO25                       emergency push button
```

See `docs/hardware-baseline-2026-08-25.md`.

## Maturity — do not overstate

Already present in current P4 code includes the command/state path, emergency latch, SD/WebUI work, emergency pixel/button configuration and PCM show-audio pin configuration.

Still to integrate/qualify under the new hardware baseline:

- Onboard C6 as production Communications Engine
- P4 RTC time service / backup behaviour
- Onboard ES8311 system-sound output
- Explicit I2S arbitration between onboard system audio and PCM show audio
- Full timeline/output engine work not already present

## Emergency hardware

```text
GPIO25  momentary button to GND, INPUT_PULLUP, pressed LOW
GPIO24  emergency NeoPixel data
```

Policy:

- First debounced press latches emergency.
- Holding does not retrigger.
- Releasing does not clear.
- A second press does not toggle/clear.
- `EMERGENCY:CLEAR` is rejected while the physical button remains asserted.
- Clearing never auto-resumes a show.

## Stage Controller microSD

The Waveshare board uses **SDMMC 4-bit**, not an SPI SD breakout.

Current `BoardConfig.h` mapping:

```text
CLK    GPIO43
CMD    GPIO44
D0     GPIO39
D1     GPIO40
D2     GPIO41
D3     GPIO42
POWER  GPIO45 active LOW
```

Runtime layout:

```text
/showduino/webui/
/showduino/shows/
/showduino/audio/
/showduino/logs/
/showduino/system/
```

Boot must continue safely if the card is unavailable; missing media should be reported as a fault/status condition rather than corrupting show state.

## Show audio — PCM5102A

Dedicated programme/show audio path:

```text
WS/LRCK  GPIO20
BCLK     GPIO21
DOUT     GPIO22
```

Use for music, dialogue, ambience and timed SFX.

## Showduino/system audio — onboard ES8311

Target local speaker path:

```text
I2C SDA     GPIO7
I2C SCL     GPIO8
I2S DSDIN  GPIO9
I2S LRCK   GPIO10
I2S ASDOUT GPIO11
I2S SCLK   GPIO12
I2S MCLK   GPIO13
PA enable  GPIO53
```

Use for short boot/ready/loaded/armed/warning/error/emergency acknowledgement sounds.

The P4 has one I2S peripheral. Firmware must explicitly own/arbitrate that resource; do not assume two independent simultaneous audio streams.

## RTC

The final Stage Controller uses the P4 RTC domain and the Waveshare rechargeable RTC battery connection.

```text
External DS3231: not part of final hardware baseline
```

Power-loss retention must be bench-qualified before it is marked confirmed.

## Communications Engine

Target:

```text
firmware/p4-c6-espnow-bridge/
```

Compatibility/rollback:

```text
firmware/c3-supermini-espnow-bridge/
```

The board integrates the C6 with the P4 for wireless expansion over SDIO. The C6 UART header is a flash/debug path; old placeholder UART pin assumptions in the C6 bring-up sketch are not the final internal transport contract.

## Arduino IDE / CLI — current P4 build

Bench configuration used successfully:

- Board: **ESP32P4 Dev Module**
- Flash Size: **16MB (128Mb)**
- PSRAM: **Enabled**
- Compatible chip revision setting for the fitted P4
- Serial: 115200

Do not build a 32 MB image for the current 16 MB board configuration.

Example Arduino CLI shape:

```text
arduino-cli compile --fqbn "esp32:esp32:esp32p4:PSRAM=enabled,FlashSize=16M" firmware/stage-engine-p4/ShowduinoStageEngineP4
arduino-cli upload -p COMx --fqbn "esp32:esp32:esp32p4:PSRAM=enabled,FlashSize=16M" firmware/stage-engine-p4/ShowduinoStageEngineP4
```

Replace `COMx` with the P4 USB serial port.

## Policy reminders

- Absolute relay states only.
- No false success for placeholder pixel/audio routes.
- Address nodes by logical device ID at the application layer.
- Publish authoritative state; Director/browser actions are requests.
- Running shows must not require Director/browser presence.
- Hardware target and firmware maturity must be documented separately.

See [`docs/architecture.md`](../../docs/architecture.md), [`docs/hardware-pinout.md`](../../docs/hardware-pinout.md), and [`docs/repository-status.md`](../../docs/repository-status.md).
