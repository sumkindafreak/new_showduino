# Local browser control (SoftAP Studio → S3 → P4)

```text
Status: IMPLEMENTED (local SoftAP path)
Authority: ESP32-P4 Show Engine
Transport: ESP32-S3 Communications Controller
Not: public internet control
```

## Architecture

```text
Browser (same SoftAP Wi-Fi)
    ↓  HTTP  http://192.168.4.1/
ESP32-S3 Communications Controller
    · SoftAP on ESP-NOW channel (default 1 / follow venue STA)
    · PROGMEM Studio assets
    · UART web tunnel WEB/ / WEBR: / WEB/BODY:
    ↓  UART 115200
ESP32-P4 Show Engine
    · validates /api/command whitelist
    · owns show / emergency / productions
```

- **P4** remains authoritative for commands, shows, state, and emergency policy.
- **S3** provides network access and transport only. It does not invent show state.
- **Director** remains a separate operator interface (ESP-NOW).
- Onboard **C6** stays unused.
- Shows continue if the browser or SoftAP disconnects (constitution Article IX).

Static Studio assets are served from **S3 PROGMEM** (not P4 SD). P4 SD remains the production/log store. This keeps the browser UI available for commissioning even if SD is busy, without making S3 the show authority.

## Setup

1. Flash Communications firmware `0.5.3+` and a current P4 build with the web tunnel.
2. Optional: copy `LocalSecrets.h.example` → `LocalSecrets.h` and set a venue SoftAP password / control token.
3. Join Wi-Fi SSID `Showduino` (WPA2). Default bench password is only used when `LocalSecrets.h` is absent — change before any public venue.
4. Open `http://192.168.4.1/` (or `http://showduino.local/` when mDNS works).
5. Use **Live** for START / PAUSE / RESUME / STOP / PANIC. Controls disable when `p4Online` is false.

Do **not** port-forward the SoftAP. Local HTTP SoftAP access is **not** internet control.

## Command API contract

`POST /api/command`  
`Content-Type: application/json`

```json
{ "cmd": "SHOW:START", "requestId": "web-abc-1" }
```

Compatible structured form (Studio maps to `cmd`):

```json
{
  "category": "show",
  "action": "start",
  "source": "studio",
  "destination": "p4",
  "priority": 0,
  "payload": null,
  "requestId": "web-abc-1"
}
```

| Field | Required | Notes |
|-------|----------|--------|
| `cmd` | yes (or category/action) | Colon command after whitelist |
| `requestId` | recommended | Opaque id; duplicates are not replayed |
| `source` / `destination` / `priority` / `payload` | no | Accepted and ignored for transport |

Initially supported actions (when P4 implements them): `STATUS:REQUEST`, `SHOW:START`, `SHOW:STOP`, `SHOW:PAUSE`, `SHOW:RESUME`, `EMERGENCY:STOP`, plus other whitelisted commissioning commands (PIXEL/AUDIO/NET/…).

`PRODUCTION:LOAD:<id>` is allowed only after P4 storage validates the id. Unsupported actions return `403` / `lifecycle: rejected`.

Remote **emergency CLEAR** is **not** exposed on the browser surface by default (`SHOWDUINO_WEBUI_ALLOW_EMERGENCY_CLEAR=0`). Assert (`EMERGENCY:STOP`) remains available. Clear stays on the Director physical path.

Optional header when `SHOWDUINO_WEBUI_CONTROL_TOKEN` is defined in `LocalSecrets.h`:

```text
X-Showduino-Token: <token>
```

Studio reads `localStorage.showduino.controlToken` if set.

### Response lifecycle

```json
{
  "ok": true,
  "cmd": "SHOW:START",
  "requestId": "web-abc-1",
  "lifecycle": "accepted",
  "duplicate": false,
  "replies": "...",
  "showState": "RUNNING",
  "emergencyActive": false,
  "note": "accepted means Show Engine dispatch — confirm via /api/show state"
}
```

| lifecycle | Meaning |
|-----------|---------|
| `accepted` | P4 validated and dispatched — **not** physical completion |
| `rejected` | Whitelist/policy/runtime rejection |
| `duplicate` | Same `requestId` seen — **action not replayed** |

Status is confirmed by polling `GET /api/show` and `GET /api/system` (~2 s). There is no WebSocket push channel in this path.

## UART tunnel

| Frame | Role |
|-------|------|
| `WEB/GET<path>` | Proxy GET |
| `WEB/POST<path>` | Proxy POST without body (legacy command path) |
| `WEB/BODY:<len>:<METHOD>:<path>\n` + bytes | JSON envelopes / deploy chunks |
| `WEBR:<status>:<bodyLen>[:mime]\n` + bytes | Length-delimited response |

Max body: 24 576 bytes (~2 s @ 115200). Comms pumps ESP-NOW / UART while waiting and drains after timeouts so desk traffic recovers. Web payloads are never forwarded to the Director. TX header+body is locked so desk writers cannot interleave mid-frame.

## Security (local)

- SoftAP WPA2 password (provision outside git via `LocalSecrets.h`)
- Optional control token for mutating APIs
- Same-origin SoftAP; **no** wildcard credentialed CORS
- Request size limits on `/api/command`
- No unrestricted raw command bridge
- No port-forward guidance

## Hardware checks (manual — not performed by CI)

- [ ] Director + browser simultaneous use
- [ ] Show continues after SoftAP client disconnect
- [ ] Reconnect does not replay prior `requestId` actions
- [ ] Emergency assert remains responsive during large WEBR/asset transfer
- [ ] SoftAP stays on ESP-NOW channel while venue STA connected (gateway mode)

## Related

- [`docs/public-https-control-proposal.md`](public-https-control-proposal.md) — public website path (not implemented here)
- [`docs/network-gateway.md`](network-gateway.md) — optional venue STA
- [`web/showduino-studio/README.md`](../web/showduino-studio/README.md)
