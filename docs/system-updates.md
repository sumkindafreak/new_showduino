# Showduino system updates

```text
Status: PHASE 2A COMMS SELF-OTA / SYSTEM-WIDE OTA NO / OTHER NODES USB
Product: Showduino 1.0.0-rc.1
Protocol: 1.0
SHDO: v2 unchanged
```

The long-term operator path remains:

```text
SYSTEM → UPDATES → CHECK FOR UPDATES → UPDATE SYSTEM
```

Phase 2A does **not** implement `UPDATE SYSTEM`. The only install action is **UPDATE COMMS**.

## What Phase 2A is

| Piece | Role |
|-------|------|
| Release manifest | `releases/showduino-1.0.0-rc.1.manifest.json` + `showduino_update_manager.h` |
| Live inventory | P4 `/api/updates` and `/api/system` `updateInventory` |
| Product check | Existing Comms GitHub Releases discovery |
| Comms self-OTA | Streamed HTTPS write to the inactive OTA slot, SHA-256, health gate, rollback |
| Maintenance | P4 `UPDATE:MAINTENANCE:ON` / `OFF`. New shows cannot start. Emergency stays live |
| Emergency gate | Existing `showduino_emergency_update_gate()` — unchanged and still locked |

Details: `docs/comms-ota.md`.

## What Phase 2A is not

- Not system-wide OTA
- Not P4 / Director / Lamp / Audio / Pixel / Emergency / MOSFET / Relay OTA
- Not signed release manifests
- Not a replacement for P4 GPIO25 hardwired emergency
- Not an SHDO cue or Studio installer

`POST /api/updates/apply` with `"component":"comms"` may start Comms self-OTA.  
Any other component returns **501** `OTA_UNAVAILABLE`.  
Generic apply without a component still returns **501** `OTA_NOT_IMPLEMENTED`.

## Emergency Node order (locked)

```text
ESTOP-01 update
        ↓
      reboot
        ↓
 healthy + linked
        ↓
ESTOP-02 update
```

Never flash, install, reboot, or deliberately take two Emergency Nodes offline together.

`ESTOP OTA CAPABLE = FALSE`.

Wireless Emergency Nodes may still **ASSERT**. They must never **CLEAR**.

## Authority

| Layer | Owns |
|-------|------|
| P4 | Show runtime, safety, maintenance authorisation, live inventory / plan. P4 image is USB-only |
| Comms | Home/venue Wi-Fi, GitHub discovery, WebUI, **its own** OTA install |
| Director | Operator Software dialog. Displays Comms update / reconnect. No firmware download |
| Studio | Inventory + Update Comms confirmation. No timeline update cue |
| Nodes | Their own firmware image, when a later OTA phase exists |

A running production must not depend on GitHub, internet, or this manager.

## Manifest

Schema `showduino-release-v1`. Product version, Protocol 1.0, SHDO 2.  
Only the Comms component may set `otaCapable: true`, and only after the dual-slot implementation exists.

## Later phases (not this pass)

Local specialist-node OTA (Phase 2B) is not started here. USB flashing has not been eliminated system-wide.
