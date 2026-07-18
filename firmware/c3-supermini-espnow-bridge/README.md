# Showduino Communications Engine — ESP32-C3 SuperMini

```text
Status: ACTIVE
Role: Showduino Communications Engine
```

Transports only. Canonical active bridge firmware.

```text
Director  --ESP-NOW-->  this C3  --UART-->  Show Engine (P4)
Node      --ESP-NOW-->  this C3  --UART-->  Show Engine (P4)
Browser   --Wi-Fi-->    this C3  --> Show Engine services   (planned)
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

Wi‑Fi AP/STA for browser access to Show Engine services is **planned**, not claimed complete in this sketch.

## Configuration notes

- Set relay peer MAC in the sketch for the current bring-up (transport detail).
- Long-term, application code uses **logical Showduino device IDs**; the Communications Engine resolves transport addresses.

## Not this project

| Path | Status |
|------|--------|
| `firmware/p4-c6-espnow-bridge/` | Experimental / incomplete |
| `firmware/espnow-bridge/` | Legacy scaffold |

Related documents: [`docs/constitution.md`](../../docs/constitution.md), [`docs/repository-status.md`](../../docs/repository-status.md).
