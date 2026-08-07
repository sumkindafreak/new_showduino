# Showduino Studio WebUI

Browser control desk for Showduino Studio — served by the Communications Engine (**SUE** / ESP32-C3).

## Architecture

```text
Browser → Wi-Fi/LAN → ESP32-C3 SUE
                        ├─ Showduino-Studio fallback AP
                        ├─ Optional local LAN station
                        ├─ Device Manager (Stage 5)
                        ├─ Command Bus (Stage 6)
                        ├─ Capability Manager + Device Router (Stage 7)
                        ├─ Time Service + DS3231 (Stage 7.5) ← authoritative clock
                        └─ UART tunnel → ESP32-P4 IAN (/api/system, /api/logs)
```

The P4 remains off the Wi-Fi control plane. SUE is the single network gateway and continues talking to the P4 over UART.

## Connect

1. Flash **P4**, **C3**, **Director**.
2. Wire DS3231 to C3: **SDA=GPIO4**, **SCL=GPIO5**, 3V3, GND.
3. Join the fallback Wi-Fi: `Showduino-Studio` / `showduino`.
4. Open `http://192.168.4.1/` for Showduino Studio.
5. Open **Network** in Studio, or `http://192.168.4.1:82/`, to configure an optional local LAN.

Internet access is not required. When a compatible LAN is configured, the fallback Showduino AP remains active so the system can still be reached if the router disappears.

## Wi-Fi / ESP-NOW channel rule

SUE uses one 2.4 GHz radio for Wi-Fi and ESP-NOW. The Showduino fabric is currently fixed to `SHOWDUINO_ESPNOW_CHANNEL = 1`, so the local 2.4 GHz router must also use **channel 1**. The provisioning UI scans networks and refuses incompatible channels rather than risking loss of Director/node control.

## Network provisioning API

The recovery-safe provisioning service runs directly on SUE on port **82**, independently of the P4 API tunnel.

| Endpoint | Description |
|----------|-------------|
| `GET :82/api/network/config` | AP + LAN status, IPs, RSSI, channel and last error |
| `GET :82/api/network/scan` | Scan nearby Wi-Fi and mark channel-1 compatible networks |
| `POST :82/api/network/config` | Save LAN SSID/password and connect |
| `POST :82/api/network/reconnect` | Retry the saved LAN |
| `DELETE :82/api/network/config` | Clear LAN settings and return to AP-only mode |
| `GET :82/` | Standalone first-time/recovery setup page |

LAN credentials are stored in ESP32 Preferences/NVS on SUE. Passwords are never returned by the status API.

## Library

Install **Adafruit RTClib** (v2.1.4+) and dependency **Adafruit BusIO** in Arduino Library Manager.

## REST API (time additions)

| Endpoint | Description |
|----------|-------------|
| `GET /api/time` | Live clock / ISO / epoch / RTC status / temperature |
| `GET /api/time/status` | RTC present/healthy/lostPower/battery/sync/drift/SQW/alarm |
| `POST /api/time/alarm` | Arm timed-show alarm (`{"epoch":…}` or `{"daily":true,"hour":h,"minute":m}`) |
| `DELETE /api/time/alarm` | Clear RTC alarm |

## WebSocket events (time additions)

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

After changing files under `web/showduino-studio`, regenerate the embedded WebUI assets before flashing firmware:

```powershell
powershell -File tools/embed-web-studio-assets.ps1
```
