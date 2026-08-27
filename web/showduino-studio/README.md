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

**Current product path (this hardware generation):**

```text
Browser  →  (not hosted on the current S3 Comms Controller)
Director → ESP-NOW → ESP32-S3 Comms Controller → UART → ESP32-P4
                         ├─ Static files from SD /showduino/webui/
                         └─ JSON APIs (/api/system, /api/logs, /api/devices)
```

The production frontend is **not** embedded in firmware. A tiny HTML fallback is shown only if the P4 SD origin is unreachable.

### Previous generation (legacy C3 / SUE)

The external ESP32-C3 SuperMini hosted SoftAP `Showduino-Studio` and proxied `/api/*` over UART. That path remains documented for the legacy C3 firmware only:

```text
Browser → Wi-Fi → ESP32-C3 SUE (HTTP radio, WebSocket :81)
                    └─ UART tunnel → ESP32-P4
```

## Connect

Current generation: flash **P4**, **S3 Comms Controller**, **Director**. Wire UART (S3 GPIO17→P4 GPIO4, S3 GPIO18←P4 GPIO5). Copy the S3 boot MAC into Director `SHOWDUINO_COMMS_MAC_*`.

There is **no external DS3231** on the current P4-module stack. Time follows the P4 internal RTC.

### Legacy C3 connect (previous hardware)

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

## DS3231 wiring (legacy SUE / C3 only)

Not used on the current P4-module generation. Retained for the previous C3 firmware:

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