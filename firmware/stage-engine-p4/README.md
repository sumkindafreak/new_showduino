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
Director --ESP-NOW--> onboard ESP32-C6 --internal SDIO/service--> this P4 Show Engine
Node     --ESP-NOW--> onboard ESP32-C6 --internal SDIO/service--> this P4 Show Engine
Browser  --Wi-Fi----> onboard ESP32-C6 ------------------------> P4 Web services
```

## C6 migration status

The final hardware target is the onboard C6, but the current Arduino Show Engine sketch still contains the previously working UART link for the **external C3 SuperMini**.

That old UART is a compatibility/rollback build while the onboard C6 is qualified.

### Important pin conflict

The current Arduino sketch maps the old external C3 UART to:

```text
P4 RX GPIO18
P4 TX GPIO17
```

The Waveshare board physically uses those same pins for the onboard C6 SDIO bus:

```text
GPIO17 = C6 SDIO D3
GPIO18 = C6 SDIO CLK
```

Therefore the external-C3 UART and onboard-C6 SDIO path **cannot be active at the same time**.

Do not graft ESP-Hosted/SDIO into the current Arduino build while it still initializes Serial1 on GPIO17/18. The migration must first make the two communications backends mutually exclusive.

### First no-risk qualification

Use the separate ESP-IDF project:

```text
firmware/p4-c6-espnow-bridge/hosted-link-qualification/
```

It runs on the P4 and leaves the factory C6 firmware untouched. It proves:

```text
P4 -> internal SDIO -> C6 -> Wi-Fi radio
```

by bringing up ESP-Hosted, reading the C6 STA MAC and running a Wi-Fi scan.

Only after that passes should we move to the custom Showduino ESP-NOW service.

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

## Known GPIO debt before onboard-audio enable

The current Arduino sketch also defines its generic `STATUS_LED_PIN` as **GPIO10**.

GPIO10 is the onboard ES8311 **LRCK/WS** signal. Under the final hardware baseline it is reserved for audio and must not remain a status LED output.

This legacy assignment must be disabled/removed before the onboard ES8311 driver is enabled.

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

The stock hosted API does not currently expose ESP-NOW to the P4 application. Showduino's final C6 implementation therefore requires a deliberate C6-side ESP-NOW extension/custom service rather than pretending ordinary hosted Wi-Fi RPC is sufficient.

## Arduino IDE / CLI — current rollback P4 build

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
- Never activate the external-C3 UART and onboard-C6 SDIO on GPIO17/18 together.

See [`docs/architecture.md`](../../docs/architecture.md), [`docs/hardware-pinout.md`](../../docs/hardware-pinout.md), and [`docs/repository-status.md`](../../docs/repository-status.md).
