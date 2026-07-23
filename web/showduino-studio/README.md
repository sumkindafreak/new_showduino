# Showduino Studio WebUI

Browser control desk for Showduino Studio — served by the Communications Engine (**SUE** / ESP32-C3).

## Architecture

```text
Browser → Wi-Fi → ESP32-C3 SUE
                    ├─ Device Manager (Stage 5)
                    ├─ Command Bus (Stage 6)
                    ├─ Capability Manager + Device Router (Stage 7)
                    ├─ Time Service + DS3231 (Stage 7.5) ← authoritative clock
                    └─ UART tunnel → ESP32-P4 IAN (/api/system, /api/logs)
```

## Connect

1. Flash **P4**, **C3**, **Director**
2. Wire DS3231 to C3: **SDA=GPIO4**, **SCL=GPIO5**, 3V3, GND
3. Join Wi-Fi: `Showduino-Studio` / `showduino`
4. Open `http://192.168.4.1/` — Devices, Commands, Capabilities, Routing, **Time**

## Library

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