# Showduino Communications Engine — ESP32-C3 SuperMini (legacy)

```text
Status: LEGACY / SUPERSEDED
Role: Previous external Communications Engine (ESP32-C3 SuperMini / SUE)
```

This firmware remains in-tree as the **previous** Communications Engine.

The current Waveshare P4-module generation uses `firmware/s3-comms-controller/` (dedicated ESP32-S3). The onboard C6 path (`firmware/p4-c6-espnow-bridge/`) is unused/reserved. Do not treat this C3 sketch as the live desk path.

```text
Director  --ESP-NOW-->  this C3  --UART-->  Show Engine (P4)
Node      --ESP-NOW-->  this C3  --UART-->  Show Engine (P4)
Browser   --Wi-Fi-->    this C3  --UART tunnel-->  Show Engine REST API
```

## Constitution

> The Communications Engine transports.

It must **not**:

- Run the show timeline
- Own authoritative show state
- Make show-level decisions
- Return success for unimplemented pixel/audio (or other) actions

## Sketch

```text
firmware/c3-supermini-espnow-bridge/ShowduinoC3SuperMiniBridge/
```

## Behaviour today

- Desk ESP‑NOW packets ↔ UART lines to the Show Engine
- Intercept Show Engine `ROUTE:RELAY:…` and forward via ESP‑NOW to the relay node
- Forward node replies as `NODE:…` on UART
- Learn Director MAC from inbound desk traffic
- **Wi‑Fi AP front door** (`Showduino-Studio`) serves Studio WebUI static assets
- Proxies `/api/*` to P4 via UART web tunnel (`protocol/showduino_web_tunnel.h`)

Connect: join **`Showduino-Studio`** / **`showduino`**, open **`http://192.168.4.1/`**

## Configuration notes

- Set relay peer MAC in the sketch for the current bring-up (transport detail).
- Long-term, application code uses **logical Showduino device IDs**; the Communications Engine resolves transport addresses.
- AP runs on ESP-NOW channel 1 so desk link stays stable.

## Not this project

| Path | Status |
|------|--------|
| `firmware/s3-comms-controller/` | **ACTIVE** current Communications Engine (dedicated ESP32-S3) |
| `firmware/p4-c6-espnow-bridge/` | UNUSED / RESERVED onboard C6 |
| `firmware/espnow-bridge/` | Legacy scaffold |

Related documents: [`docs/constitution.md`](../../docs/constitution.md), [`docs/repository-status.md`](../../docs/repository-status.md).
