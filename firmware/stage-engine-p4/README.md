# Showduino Show Engine — Stage Controller (ESP32-P4)

```text
Status: ACTIVE
Role: Showduino Show Engine
Product: Stage Controller (ESP32-P4)
```

Canonical active Show Engine firmware. Folder name `stage-engine-p4` is temporary; rename planned.

```text
Director --ESP-NOW--> Communications Engine --UART--> this Show Engine
Node     --ESP-NOW--> Communications Engine --UART--> this Show Engine
```

## Constitution

> The Show Engine decides.

Owns (target): authoritative show state, timeline/cues, project storage, configuration, safety policy, Web UI / Web API / WebSocket, and node command lifecycle (accept vs complete).

## Maturity — do not overstate

Current sketch is an **early command hub**:

- Parses colon-text requests over UART
- Tracks simple flags (e.g. show running, emergency lock)
- Routes relay (and stub) work through the Communications Engine
- Returns basic ACK / status lines

It does **not** yet implement the full timeline engine, primary project store, DMX, or real pixel/audio engines.

**Stage 4 WebUI:** REST API (`/api/system`, `/api/devices`, `/api/logs`) is implemented on P4. Browser access is via the C3 Wi-Fi front door (static assets on C3, API proxied over UART). See `web/showduino-studio/README.md`.

**Stage Controller SD:** Optional SPI microSD mount (`SHOWDUINO_SD_ENABLED`). Creates `/showduino/...` folders, reports mount status in `/api/system`. Defaults target Espressif/Waveshare P4 Function EV pins — edit `BoardConfig.h` for your board. Boot continues if the card is missing.

**Emergency Neopixel:** optional local strip on the Stage Controller turns solid white on E-stop (`BoardConfig.h`: pin/count/brightness). Requires **Adafruit NeoPixel** library. Remote PIXEL nodes remain unsupported until that engine exists.

## Sketch

```text
firmware/stage-engine-p4/ShowduinoStageEngineP4/
```

### SD card layout (FAT32)

```text
/showduino/www/           Studio WebUI (optional on card)
/showduino/shows/packages/
/showduino/logs/
/showduino/system/
...
```

Pins (defaults in `BoardConfig.h`):

```text
SCK=43  MISO=39  MOSI=44  CS=42  POWER=45 (active LOW)
```

Link UART remains GPIO5 (RX) / GPIO6 (TX) to the C3.
## Policy reminders for later firmware work

- Absolute relay states only (no distributed `TOGGLE`)
- No false success for placeholder pixel/audio routes
- Address nodes by logical device ID at the application layer
- Publish state changes; treat Director/browser input as requests
- Running show must not require Director or browser presence

See [`docs/constitution.md`](../../docs/constitution.md), [`docs/architecture.md`](../../docs/architecture.md), and [`docs/repository-status.md`](../../docs/repository-status.md).
