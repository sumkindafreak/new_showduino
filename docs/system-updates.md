# Showduino system updates

```text
Status: PHASE 1 FOUNDATION / OTA NOT IMPLEMENTED / NOT PHYSICALLY PROVEN
Product: Showduino 1.0.0-rc.1
Protocol: 1.0
SHDO: v2 unchanged
```

The long-term operator path is:

```text
SYSTEM → UPDATES → CHECK FOR UPDATES → UPDATE SYSTEM
```

Phase 1 does **not** install firmware. USB flash remains the only supported way to change component images.

## What Phase 1 is

| Piece | Role |
|-------|------|
| Release manifest | `releases/showduino-1.0.0-rc.1.manifest.json` + `showduino_update_manager.h` |
| Live inventory | P4 `/api/updates` and `/api/system` `updateInventory` |
| Product check | Existing Comms GitHub Releases discovery |
| Update plan | Ordered steps. Apply always blocked |
| Emergency gate | Existing `showduino_emergency_update_gate()` |

## What Phase 1 is not

- Not system-wide OTA
- Not automatic install
- Not a claim that wireless update is safe
- Not a replacement for P4 GPIO25 hardwired emergency

`POST /api/updates/apply` returns **501** `OTA_NOT_IMPLEMENTED`.

## Emergency Node order (locked)

```text
ESTOP-01 update
        ↓
      reboot
        ↓
 healthy + linked
        ↓
ESTOP-02 update
        ↓
 healthy + linked
        ↓
ESTOP-03 ...
```

Never flash, install, reboot, or deliberately take two Emergency Nodes offline together.

The System Update Manager calls `showduino_emergency_update_gate()`. It does not own a second gate.

If the gate fails or times out:

```text
SAFETY NODE UPDATE FAILED
```

Stop. Do not continue to ESTOP-(n+1).

`HEALTHY` = booted, NC input readable, no local node fault.  
`LINKED` = Comms/P4 have a fresh announce/heartbeat.

An Emergency Node offline during an update remains a **SAFETY NODE FAULT**, not automatic global emergency.

Wireless Emergency Nodes may still **ASSERT**. They must never **CLEAR**.

## Authority

| Layer | Owns |
|-------|------|
| P4 | Show runtime, safety, live node inventory / plan |
| Comms | Home/venue Wi-Fi, GitHub product discovery, WebUI proxy |
| Director | Operator Software dialog. No OTA install |
| Studio | Inventory display. No timeline update cue |
| Nodes | Their own firmware image, when a later OTA phase exists |

A running production must not depend on GitHub, internet, or this manager.

## Manifest

Schema `showduino-release-v1`. Product version, Protocol 1.0, SHDO 2, per-component firmware, `otaCapable: false`.

Do not treat arbitrary `main` commits as a released product.

## Later phases (not this pass)

Partition layout, image hash, interrupted recovery, rollback, per-component targeting, mixed-version recovery, and physical OTA proof. Those must exist before anyone claims `UPDATE SYSTEM` works.
