# Showduino Show Engine — Stage Controller (ESP32-P4)

```text
Status: ACTIVE
Role: Showduino Show Engine
Product: Stage Controller (ESP32-P4)
```

Canonical active Show Engine firmware. Folder name `stage-engine-p4` is temporary; rename planned.

```text
Director --ESP-NOW--> ESP32-S3 Comms Controller --UART--> this Show Engine
Node     --ESP-NOW--> ESP32-S3 Comms Controller --UART--> this Show Engine
```

## Constitution

> The Show Engine decides.

Owns (target): authoritative show state, timeline/cues, project storage, configuration, safety policy, Web API, and node command lifecycle (accept vs complete). The Communications S3 hosts the static browser WebUI from PROGMEM and proxies API GETs here.

## Maturity — do not overstate

Current sketch is an authoritative command/runtime hub:

- Parses colon-text requests over UART
- Tracks simple flags (e.g. show running, emergency lock)
- Routes relay (and stub) work through the Communications Engine
- Returns basic ACK / status lines
- Discovers versioned productions under `/showduino/productions/`
- Validates and transactionally loads persistent TEST/LOG timelines into RAM
- Runs loaded timelines independently of Director, browser, Wi-Fi, or internet
- Optional onboard Ethernet (IP101GRI / RMII) with DHCP or static IP
- Isolated E1.31 / sACN test receiver (Universe 1 default, observation only)
- Authoritative Audio Node tracker (`AUDIO:NODE:*` → `ROUTE:AUDIO:`, confirmed lifecycle)
- P4 onboard ES8311 system/safety audio (`AUDIO:STATUS`, `AUDIO:TEST:*`, `AUDIO:STOP`)

It does **not** yet implement broader production assets, physical cue engines,
logical target routing, production DMX/E1.31 mapping, or a complete authoring/project-management system.

Ethernet is optional. Missing cable / DHCP / E1.31 must not fail boot or stop a local show.

**Stage 4 WebUI:** REST API (`/api/system`, `/api/devices`, `/api/logs`) is implemented on P4. The Communications S3 is the canonical static host (PROGMEM). P4 static SD serving is compiled out (`SHOWDUINO_P4_STATIC_WEBUI 0`) but the SD serve path is retained behind that flag. See `web/showduino-studio/README.md`.

**Stage Controller SD:** Onboard microSD on **SDMMC Slot 0**, GPIO39–45 (`SHOWDUINO_SD_ENABLED`). Creates `/showduino/...` folders, reports mount status in `/api/system`. Boot continues if the card is missing. Do not move the card onto the internal C6 SDIO pins.

**Emergency Neopixel:** optional local strip on the Stage Controller turns solid white on E-stop (`BoardConfig.h`: pin/count/brightness). Requires **Adafruit NeoPixel** library. Remote PIXEL nodes remain unsupported until that engine exists.

## Sketch

```text
firmware/stage-engine-p4/ShowduinoStageEngineP4/
```

### Arduino IDE / CLI flash (this hardware)

The Stage Controller P4 on this bench reports **16 MB** SPI flash (`Detected size(16384k)`).  
Do **not** build or flash a **32 MB** image. That writes `32768k` into the binary header; ESP-IDF then aborts in `init_flash` and reboot-loops.

Arduino IDE:

- Board: **ESP32P4 Dev Module**
- Flash Size: **16MB (128Mb)**
- PSRAM: **Enabled**
- Partition Scheme: **Default** (not any 32M FAT / 13MB APP scheme)
- Chip Variant: **v3.00 or newer** if the ROM banner is `ESP-ROM:esp32p4-eco2-…`

arduino-cli:

```text
arduino-cli compile --fqbn "esp32:esp32:esp32p4:PSRAM=enabled,FlashSize=16M,ChipVariant=postv3" firmware/stage-engine-p4/ShowduinoStageEngineP4
arduino-cli upload -p COMx --fqbn "esp32:esp32:esp32p4:PSRAM=enabled,FlashSize=16M,ChipVariant=postv3" firmware/stage-engine-p4/ShowduinoStageEngineP4
```

Replace `COMx` with the P4 USB serial port.

### SD card layout (FAT32)

```text
/showduino/productions/   Authoritative runtime production folders
/showduino/config/        Versioned persistent settings
/showduino/audio/system/  P4 system/safety WAV (boot, emergency, beep, tone, error, accepted, complete; shutdown reserved)
/showduino/logs/          Bounded event logs
/showduino/diagnostics/   RUN:TEST exports
/showduino/backups/       Config snapshots
/showduino/system/        storage-version / last-boot
/showduino/webui/         Legacy optional copy (canonical UI is S3 PROGMEM)
/showduino/shows/         Legacy packages
```

See [`docs/p4-sd-storage.md`](../../docs/p4-sd-storage.md). SD is the persistent backbone, not the safety backbone. USB: `STORAGE:STATUS`, `STORAGE:LIST`, `STORAGE:CHECK`, `STORAGE:BACKUP`.

SDMMC pins (defaults in `BoardConfig.h`):

```text
D0=39  D1=40  D2=41  D3=42  CLK=43  CMD=44  POWER=45 (active LOW)
```

Comms UART (dedicated ESP32-S3; P4 pins unchanged):

```text
P4 GPIO4 RX  <-  S3 GPIO17 TX
P4 GPIO5 TX  ->  S3 GPIO18 RX
115200 8N1, newline-framed ASCII
```

Reserved onboard C6 infrastructure (do not allocate): GPIO6, GPIO14–19, GPIO54. The onboard C6 is unused reserved hardware.

Onboard Ethernet (Waveshare IP101GRI, PHY addr 1): MDC=31 MDIO=52 RST=51 TX_EN=49 TXD=34/35 RXD=29/30 CRS_DV=28 REF_CLK=50. Config: `/showduino/config/network.json`. USB: `NET:STATUS`, `E131:STATUS`, `E131:CHANNELS`.

Authoritative pin map: [`docs/final-hardware-architecture.md`](../../docs/final-hardware-architecture.md).

## Local USB maintenance console

The P4 USB Serial/debug connection is an **additional** command input. It does **not** replace:

```text
Director --ESP-NOW--> ESP32-S3 Comms Controller --UART GPIO4/5--> this Show Engine
```

Open Arduino Serial Monitor (or any terminal) on the P4 USB port:

- Baud: **115200** (same as existing debug output)
- Line ending: **Newline** or **Both NL & CR**
- Type Showduino colon-text commands and press Enter

USB and Comms UART both call the same Stage Engine command dispatcher. Authoritative state changes (for example `EMERGENCY:STOP` or `SHOW:START`) still notify the Director over UART if the comms controller is up.

`HELP` and local `STATUS:REQUEST` replies stay on USB Serial. They are not forwarded to the Director.

Physical emergency is a momentary pushbutton from GPIO25 to GND. Released/HIGH is healthy. Pressed/LOW latches emergency. Release does not clear. USB `EMERGENCY:CLEAR` is a bench maintenance path and still refuses a held button. Director clearance is dual-action: a 3 s physical hold sends `EMERGENCY:CLEAR_REQUEST`, then `EMERGENCY:CLEAR_CONFIRM` succeeds only while that request is pending, not timed out, and the button is released again. Clearing emergency does not restart the show.

Implemented console commands:

```text
HELP
STATUS:REQUEST
SHOW:START
SHOW:STOP
SHOW:PAUSE
SHOW:RESUME
SHOW:LOAD:<name>
PRODUCTION:LIST
PRODUCTION:LOAD:<id>
PRODUCTION:UNLOAD
PRODUCTION:STATUS
EMERGENCY:STOP
EMERGENCY:CLEAR
STORAGE:STATUS
STORAGE:LIST
STORAGE:CHECK
STORAGE:BACKUP
AUDIO:STATUS
AUDIO:TEST:BOOT | EMERGENCY | BEEP | TONE | ERROR | ACCEPTED | COMPLETE
AUDIO:STOP
AUDIO:NODE:PLAY:<path>
```

Existing debug lines (`[SD]`, `[AUDIO]`, `[COMMS]`, `[WEB]`, `[Runtime]`, `[ESTOP]`) continue on the same Serial port.

## Showduino Plug-in Bus

3.3V I²C peripheral bus on the Waveshare I²C header:

```text
SDA = GPIO7
SCL = GPIO8
100 kHz
```

USB commands: `PLUGIN:SCAN`, `PLUGIN:LIST`, `PLUGIN:STATUS`, `PLUGIN:INFO:<instance|address>`.

Discovery reports chip identity first. Operational roles come from `/showduino/config/plugin-bus.json` (or the fixed onboard ES8311 role). An SX1509 is `SX1509 - Unconfigured` until that file assigns `DIGITAL_INPUTS`, `DIGITAL_OUTPUTS`, or `DIGITAL_IO`. Unknown devices are listed, not treated as faults. See [`docs/plugin-bus.md`](../../docs/plugin-bus.md).

## Policy reminders for later firmware work

- Absolute relay states only (no distributed `TOGGLE`)
- No false success for placeholder pixel/audio routes
- Address nodes by logical device ID at the application layer
- Publish state changes; treat Director/browser input as requests
- Running show must not require Director or browser presence

Onboard C6 remains unused/reserved. Qualification sketches live under `firmware/p4-c6-espnow-bridge/` and are not the live Communications Engine.

See [`docs/constitution.md`](../../docs/constitution.md), [`docs/architecture.md`](../../docs/architecture.md), [`docs/hardware-pinout.md`](../../docs/hardware-pinout.md), and [`docs/repository-status.md`](../../docs/repository-status.md).
