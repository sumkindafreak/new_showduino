# Showduino Studio WebUI

Browser control desk for Showduino Studio.

## Source vs runtime

| Copy | Location |
|------|----------|
| **Source (version control)** | `web/showduino-studio/` |
| **Runtime (P4 SD card)** | `D:\showduino\webui\` → ESP32 `/showduino/webui/` |

Deploy to the inserted P4 SD card without formatting it:

```powershell
powershell -File tools/deploy-webui-to-sd.ps1
```

## Final target architecture

```text
Browser
   → Wi-Fi
Stage Controller onboard ESP32-C6
   → P4 Show Engine services
      ├─ static files from SD /showduino/webui/
      ├─ REST API
      └─ WebSocket/live state
```

The production frontend is stored on the P4 SD card. The Communications Engine is transport; the P4 Show Engine remains authoritative.

## Communications migration note

The repository still contains the previously working WebUI front door through the **external C3 SuperMini + UART tunnel**. That implementation is retained as compatibility/reference code while the onboard C6 gains equivalent Wi-Fi/WebUI transport.

Final hardware does not require the separate C3 board.

## Time service migration

The old Studio implementation described a C3-hosted **DS3231** service. That is no longer the final hardware architecture.

Final target:

```text
ESP32-P4 RTC / system time
    → P4 Show Engine time service
    → Studio / Director clients
```

No DS3231 wiring is required in the final Stage Controller.

RTC backup/power-loss retention must not be claimed until the P4 RTC/VBAT behaviour has been qualified on the actual board/firmware.

Existing API names such as `/api/time` may remain useful, but the backend authority moves to the P4.

Recommended target endpoints:

| Endpoint | Description |
|----------|-------------|
| `GET /api/time` | Current Show Engine clock / ISO / epoch / source |
| `GET /api/time/status` | P4 RTC/system-time health and synchronisation status |
| `POST /api/time/alarm` | Timed-show scheduling request where supported |
| `DELETE /api/time/alarm` | Clear scheduled alarm where supported |

DS3231-only fields such as chip temperature/SQW state should not be presented as canonical P4 time-service data.

## Connection target

Once onboard C6 migration is complete:

1. Flash P4 Show Engine.
2. Bring up/flash onboard C6 Communications Engine using the documented recovery procedure.
3. Flash Director with the onboard C6 ESP-NOW peer MAC.
4. Join the Showduino Wi-Fi network exposed by the onboard C6.
5. Open the Studio WebUI address provided by the Communications Engine configuration.

Until that migration is complete, `firmware/c3-supermini-espnow-bridge/` remains the compatibility implementation for the previous `Showduino-Studio` AP/tunnel behaviour.

## Regenerate embedded/fallback assets

```powershell
powershell -File tools/embed-web-studio-assets.ps1
```

## Related docs

- `docs/architecture.md`
- `docs/hardware-baseline-2026-08-25.md`
- `docs/final-hardware-architecture.md`
- `docs/repository-status.md`
