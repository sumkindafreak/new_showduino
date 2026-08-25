# Showduino Show Engine — Stage Controller (ESP32-P4)

```text
Status: ACTIVE
Role: Showduino Show Engine
Product: Stage Controller
Board: Waveshare ESP32-P4-Module-DEV-KIT
Primary development workflow: Arduino IDE
```

Canonical active Show Engine firmware. Folder name `stage-engine-p4` is historical and may be renamed later.

## Target product path

```text
Director --ESP-NOW--> onboard ESP32-C6 --internal SDIO/service--> this P4 Show Engine
Node     --ESP-NOW--> onboard ESP32-C6 --internal SDIO/service--> this P4 Show Engine
Browser  --Wi-Fi----> onboard ESP32-C6 ------------------------> P4 Web services
```

## C6 migration status

The final hardware target is the onboard C6, but the current Show Engine sketch still contains the previously working UART link for the external C3 SuperMini as rollback/reference code.

### Important pin conflict

The old external-C3 UART uses:

```text
P4 RX GPIO18
P4 TX GPIO17
```

The onboard C6 uses those same pins as:

```text
GPIO17 = C6 SDIO D3
GPIO18 = C6 SDIO CLK
```

Therefore the external-C3 UART and onboard-C6 SDIO path **cannot be active at the same time**. Production migration must feature-gate/remove the GPIO17/18 UART before the main Show Engine enables the onboard C6.

## First no-risk C6 qualification — Arduino IDE

Open and flash this **P4** sketch:

```text
firmware/p4-c6-espnow-bridge/arduino-hosted-link-qualification/ShowduinoP4C6HostedQualification/ShowduinoP4C6HostedQualification.ino
```

It leaves the factory C6 firmware untouched and proves:

```text
P4 -> internal SDIO -> onboard C6 -> Wi-Fi radio
```

using Arduino-ESP32's hosted Wi-Fi support. It sets the C6 pins, starts the hosted interface, reads the C6 STA MAC and performs a Wi-Fi scan.

The older `hosted-link-qualification/` ESP-IDF project is retained only as a lower-level reference/alternate diagnostic.

## Constitution

> The Show Engine decides.

The P4 owns authoritative show/runtime/emergency state, timeline/cues as implemented, project/runtime storage, safety policy, Web services, node command lifecycle, local Stage Controller outputs and system time.

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

## Maturity

Already present includes the command/state path, emergency latch, SD/WebUI work, emergency pixel/button configuration and PCM show-audio pin configuration.

Still to integrate/qualify:

- Onboard C6 as production Communications Engine
- P4 RTC time service / backup behaviour
- Onboard ES8311 system-sound output
- Explicit I2S arbitration between onboard system audio and PCM show audio
- Full timeline/output work not already present

## Known GPIO debt before onboard-audio enable

The current Arduino Show Engine sketch defines its old generic `STATUS_LED_PIN` as **GPIO10**.

GPIO10 is the onboard ES8311 **LRCK/WS** signal. It is reserved for audio and must not remain a status LED output before the onboard codec is enabled.

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

The Waveshare board uses SDMMC 4-bit:

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

## Show audio — PCM5102A

```text
WS/LRCK  GPIO20
BCLK     GPIO21
DOUT     GPIO22
```

Use for music, dialogue, ambience and timed SFX.

## Showduino/system audio — onboard ES8311

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

The P4 has one I2S peripheral. Firmware must arbitrate ownership rather than assume two independent simultaneous audio streams.

## RTC

The final Stage Controller uses the P4 RTC domain and Waveshare rechargeable RTC battery connection.

```text
External DS3231: not part of final hardware baseline
```

Power-loss retention must be bench-qualified.

## Communications Engine

Target:

```text
firmware/p4-c6-espnow-bridge/
```

Compatibility/rollback reference:

```text
firmware/c3-supermini-espnow-bridge/
```

The stock hosted API does not currently expose ESP-NOW directly to the P4 application, so Showduino's final C6 path requires a deliberate C6-side ESP-NOW service/extension with a matching internal transport to the P4.

## Arduino IDE — current P4 baseline

Use:

```text
Board: ESP32P4 Dev Module
ESP32 board package: 3.3.x or newer
Flash Size: 16MB (128Mb)
PSRAM: Enabled
Serial: 115200
```

Do not build a 32 MB image for the current 16 MB Stage Controller.

## Policy reminders

- Absolute relay states only.
- No false success for placeholder routes.
- Address nodes by logical device ID at the application layer.
- Director/browser actions are requests; P4 state is authoritative.
- Running shows must not require Director/browser presence.
- Never activate external-C3 UART and onboard-C6 SDIO on GPIO17/18 together.

See [`docs/architecture.md`](../../docs/architecture.md), [`docs/hardware-pinout.md`](../../docs/hardware-pinout.md), and [`docs/repository-status.md`](../../docs/repository-status.md).
