# Showduino Studio WebUI

Browser control desk for Showduino Studio.

## Source vs runtime

| Copy | Location |
|------|----------|
| **Source (version control)** | `web/showduino-studio/` |
| **Runtime (P4 SD card)** | `D:\showduino\webui\` → ESP32 `/showduino/webui/` |

Deploy to the inserted P4 SD card (does not format the card):

```powershell
powershell -File tools/deploy-webui-to-sd.ps1
```

## Architecture

```text
Browser → Wi-Fi → ESP32-C3 SUE (HTTP radio, WebSocket :81)
                    └─ UART tunnel → ESP32-P4
                         ├─ Static files from SD /showduino/webui/
                         └─ JSON APIs (/api/system, /api/logs, /api/devices)
```

The production frontend is **not** embedded in firmware. A tiny HTML fallback is shown only if the P4 SD origin is unreachable.

## Connect

1. Flash **P4**, **C3**, **Director**
2. Wire DS3231 to C3: **SDA=GPIO4**, **SCL=GPIO5**, 3V3, GND
3. Join Wi-Fi: `Showduino-Studio` / `showduino`
4. Open `http://192.168.4.1/`

## REST API (additions)

Install **Adafruit RTClib** (v2.1.4+) and dependency **Adafruit BusIO** in Arduino Library Manager.

## REST API (additions)

| Endpoint | Description |
|----------|-------------|
| `GET /api/time` | Live clock / ISO / epoch / RTC status / temperature |
| `GET /api/time/status` | RTC present/healthy/lostPower/battery/sync/drift/SQW/alarm |
| `POST /api/time/alarm` | Arm timed-show alarm (`{"epoch":…}` or `{"daily":true,"hour":h,"minute":m}`) |
| `DELETE /api/time/alarm` | Clear RTC alarm |

## WebSocket events (additions)

`time.updated` (1 Hz) · `time.sync` · `time.unsynced` · `rtc.status` · `time.alarm` · `time.alarm.armed` · `time.alarm.cleared`

## DS3231 wiring (SUE / C3)

| RTC pin | C3 GPIO |
|---------|---------|
| SDA | 4 |
| SCL | 5 |
| SQW / INT / DS | **6** (timed-show alarm interrupt) |
| VCC / GND | 3V3 / GND |
| 32K | unused |

## Regenerate embedded assets

```powershell
powershell -File tools/embed-web-studio-assets.ps1
```