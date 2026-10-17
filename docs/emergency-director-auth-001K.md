# MISSION 001K — authenticated Director clear authority and clearance race

Status: in progress. Sections 1–2 were written **before** any 001K code change.

Branch `fix/emergency-persistent-latch`, PR #33 (draft, do not merge). Starting head
`b3b3f95` (001J). Not hardware tested. Not a certified safety function.

## 1. Inventory: every path that can request or confirm an emergency clear (pre-001K)

Trust chain for operator commands: Director ESP32-S3 → ESP-NOW (desk packet, 95-char
command) → Comms ESP32-S3 (untrusted relay) → UART (line ≤ 240 chars at the P4) → P4.

| # | Entry point | Code | Reaches P4 as | P4 source | Pre-001K check |
|---|---|---|---|---|---|
| E1 | Director touchscreen "E-CLEAR" (Live page, Settings page, overlay) | `ShowduinoUi.h` → `handleUiCommand("EMERGENCY:CLEAR")` → `sendToStage` | `EMERGENCY:CLEAR` | Comms | none (plain text) |
| E2 | Director clear dialog "Confirm" | `DirectorEmergencyClearDialog` → `onEmergencyClearConfirm()` → `sendToStage` | `EMERGENCY:CLEAR_CONFIRM` | Comms | none |
| E3 | Director OS2 shell command service `EmergencyClear` | `.ino` `CT::EmergencyClear` → `sendToStage("EMERGENCY:CLEAR")` | `EMERGENCY:CLEAR` | Comms | none |
| E4 | Director USB console | `handleUsbLine()` forwards any unknown line with `sendToStage` | any text | Comms | none |
| E5 | Comms S3 itself (compromised or buggy) | `commsUartWriteLine()` | any text | Comms | none — **J-05** |
| E6 | Comms S3 Web UI | `CommsWebServerBase.h` `SHOWDUINO_WEBUI_ALLOW_EMERGENCY_CLEAR` (default 0) | `CLEAR_CONFIRM` / `CLEAR_CANCEL` | Comms | compile flag only |
| E7 | P4 web API over the UART body channel | `WebApiHandlerBase.h` `webCommandAllowed()` same flag (undefined by default) | `CLEAR_CONFIRM` / `CLEAR_CANCEL` | Comms | compile flag only |
| E8 | Show timeline cues | `timelineDispatchCommand()` → `handleCommand(cmd)` | any command in a `.shdo` cue | Comms | none |
| E9 | P4 USB console | `handleCommand(..., LocalUsb)` | `EMERGENCY:CLEAR` → `clearEmergencyStop()`; `CLEAR_CONFIRM` → confirm | LocalUsb | physical access to the P4 |
| E10 | Internal | `emergencyLatchReconcile()` writes CLEAR only when the RAM latch is already clear | — | — | not an entry point |

`CommandSource` only distinguishes the P4's own USB port from everything that arrives
on the UART. It is not authentication: the Director, the S3, the S3 web UI, the P4 web
API and show cues all look identical (`Comms`).

Gates every path already had (001E–001J): the physical GPIO25 loop must be released;
the request must be pending and not older than 12 s; any new assertion supersedes it;
the 001J station clear decision (pressed/unverified/offline stations, storage, table
overflow, fresh proofs from every provisioned station).

## 2. Trust decisions for 001K

| Authority | Who | How it is proven |
|---|---|---|
| Activate emergency | anyone (GPIO25, USB, Director, stations, show) | none needed — activation is never delayed |
| Operator clear (normal) | the enrolled Director, by touch | HMAC-signed request and confirm, end to end to the P4 |
| Maintenance clear | person at the P4 USB port | physical access (documented, logged) |
| Station release | each provisioned C3 | 001J station AUTH |
| Comms S3 | nobody | relays only; cannot originate any authority |
