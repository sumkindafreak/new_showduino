# Showduino Command Protocol

Canonical documentation for the shared protocol package under `protocol/`.

**Ownership:** architecture / shared protocol (not a single board firmware).  
**Constitution:** Show Engine decides · Communications Engine transports · Director commands and displays · Nodes act.

Transport topology (**current, unchanged**):

```text
Director ESP32-S3  → ESP-NOW →  Communications Engine ESP32-C3  → UART →  Show Engine ESP32-P4
Showduino Node     → ESP-NOW →  Communications Engine ESP32-C3  → UART →  Show Engine ESP32-P4
```

ESP-NOW and UART are **transports**. Application messages describe **intent**. MAC addresses must not appear in application-level message meaning.

---

## Status legend

| Label | Meaning |
|-------|---------|
| **Implemented now** | On the live wire / in shared headers used by active firmware |
| **Compatibility layer** | Legacy colon-text retained so the stack keeps working |
| **Planned later** | Catalog / model only — not claimed as live behaviour |

---

## Stage 3 authoritative state (implemented now)

See also [`docs/state-synchronisation.md`](state-synchronisation.md).

### Preferred relay requests

```text
RELAY:<channel>:ON
RELAY:<channel>:OFF
```

`RELAY:<channel>:TOGGLE` is **deprecated**. Show Engine rejects it when channel state is `UNKNOWN`.

### Lifecycle (relay)

```text
ACCEPTED:RELAY:<seq>:<channel>:ON|OFF   → request accepted / forwarded (not completed)
REJECTED:RELAY:<channel>:<REASON>       → not forwarded
FAILED:RELAY:<channel>:<REASON>         → pending cleared; last confirmed retained
STATE:RELAY:<channel>:ON|OFF|UNKNOWN|FAULT  → authoritative display
```

### Show / emergency / node

```text
STATE:SHOW:IDLE|PLAYING|EMERGENCY
STATE:EMERGENCY:ACTIVE|CLEAR
STATE:NODE:RELAY:ONLINE|OFFLINE|UNKNOWN|FAULT
```

### Snapshot

```text
STATUS:REQUEST
→ SNAPSHOT:BEGIN
→ STATE:SHOW:…
→ STATE:EMERGENCY:…
→ STATE:NODE:RELAY:…
→ STATE:RELAY:1:… … STATE:RELAY:8:…
→ SNAPSHOT:END
```

### Placeholders

```text
UNSUPPORTED:PIXEL:…
UNSUPPORTED:AUDIO:…
NOT_IMPLEMENTED:SHOW:PAUSE…
NODE_UNAVAILABLE:PIXEL|AUDIO
```

---

## Version policy

Defined in `protocol/showduino_protocol_version.h`.

| Symbol | Role |
|--------|------|
| `SHOWDUINO_PROTOCOL_VERSION_MAJOR` | Breaking wire changes |
| `SHOWDUINO_PROTOCOL_VERSION_MINOR` | Backward-compatible package additions |
| `SHOWDUINO_DESK_WIRE_VERSION` | Value in desk packet `version` field (currently `1`) |

Rules:

* Major mismatch → **reject** the packet.
* Minor is package metadata for protocol v1; desk wire still carries a single `uint16_t` equal to major (`1`).
* Unknown major versions must be rejected.
* Newer minor versions may be accepted only when required fields remain compatible (future).
* Reserved / unused fields must be zero-initialized.
* Validate magic, version, and size before using payload data.

Live packet format is protocol **version 1**.

---

## Packets (implemented now — wire compatible)

### Desk packet — Director ↔ Communications Engine (ESP-NOW)

Canonical type: `ShowduinoDeskPacket` (alias `ShowduinoEspNowPacket`).

| Offset (typical) | Field | Type | Notes |
|------------------|-------|------|-------|
| 0 | `magic` | `uint32_t` | `0x5348444F` (`SHDO`) |
| 4 | `version` | `uint16_t` | Wire value `1` |
| 6 | `sequence` | `uint16_t` | Sender sequence |
| 8 | `sentMillis` | `uint32_t` | `millis()` snapshot |
| 12 | `command` | `char[96]` | NUL-terminated legacy colon text |

* Expected `sizeof` = **108** bytes.
* Struct is **not packed**; natural alignment (matches ESP32 / typical hosts).
* Historical name in Director code: `ShowduinoEspNowPacket` (typedef to desk packet).

### Node packet — Communications Engine ↔ Node (ESP-NOW)

Canonical type: `ShowduinoNodePacket`.

| Field | Type | Notes |
|-------|------|-------|
| `nodeType` | `char[16]` | Legacy category string, e.g. `RELAY` |
| `command` | `char[96]` | NUL-terminated legacy colon text |
| `sequence` | `uint32_t` | Sequence / echo id |

* Expected `sizeof` = **116** bytes.
* **No magic on the wire today** (v1). Node packet magic is **planned** for a future major version.
* C3 discriminates desk vs node by received length.

---

## Application message catalog

Defined in `protocol/showduino_message_types.h` as `ShowduinoMessageType`.

Transport-independent intent IDs include (non-exhaustive):

### System and link

`HELLO`, `HELLO_ACK`, `HEARTBEAT`, `HEARTBEAT_ACK`, `STATUS_REQUEST`, `STATE_SNAPSHOT`, `SYSTEM_WARNING`, `SYSTEM_FAULT`

### Show control

`SHOW_START_REQUEST`, `SHOW_STOP_REQUEST`, `SHOW_PAUSE_REQUEST`, `SHOW_RESUME_REQUEST`, `SHOW_STATE_CHANGED`, `SHOW_POSITION_CHANGED`

### Cue control

`CUE_TRIGGER_REQUEST`, `CUE_TRIGGER_ACCEPTED`, `CUE_TRIGGER_REJECTED`, `CUE_STARTED`, `CUE_COMPLETED`, `CUE_FAILED`

### Node control

`NODE_COMMAND`, `NODE_COMMAND_ACCEPTED` / `REJECTED` / `STARTED` / `COMPLETED` / `FAILED`, `NODE_HEARTBEAT`, `NODE_STATE_CHANGED`, `NODE_UNAVAILABLE`

### Relay control

`RELAY_SET_REQUEST`, `RELAY_STATE_CHANGED`  
`RELAY_TOGGLE_DEPRECATED` — recognized for compatibility only; **not** the preferred operation.

### Emergency

`EMERGENCY_ACTIVATE_REQUEST`, `EMERGENCY_CLEAR_REQUEST`, `EMERGENCY_STATE_CHANGED`

### Capability / support

`CAPABILITY_QUERY`, `CAPABILITY_REPORT`, `COMMAND_UNSUPPORTED`, `NOT_IMPLEMENTED`

Message identifiers must **not** be named after transports (no `UART_PACKET`, `ESPNOW_COMMAND`, etc.).

**On the wire today:** colon-delimited text (**compatibility layer**). Full structured binary framing of these enums is **planned later**.

---

## Legacy string compatibility (on wire today)

Constants: `protocol/showduino_legacy_strings.h`. Mapper: `showduino_legacy_map_command()`.

| Legacy string | Catalog meaning |
|---------------|-----------------|
| `SHOW:START` | `SHOW_START_REQUEST` |
| `SHOW:STOP` / `STOP:ALL` | `SHOW_STOP_REQUEST` |
| `EMERGENCY:STOP` | `EMERGENCY_ACTIVATE_REQUEST` |
| `EMERGENCY:CLEAR` | `EMERGENCY_CLEAR_REQUEST` |
| `RELAY:n:ON` / `OFF` | `RELAY_SET_REQUEST` |
| `RELAY:n:TOGGLE` | `RELAY_TOGGLE_DEPRECATED` (still executed by relay node firmware) |
| `HELLO` / `HEARTBEAT` | `HELLO` / `HEARTBEAT` |
| `STATUS:REQUEST` | `STATUS_REQUEST` |

### Legacy transport envelopes (not permanent application vocabulary)

| Token | Role |
|-------|------|
| `ROUTE:<type>:` | Show Engine → Comms: deliver payload to a node category |
| `NODE:` | Comms → Show Engine: node reply envelope |
| `ACK:` / `ERR:` / `STATUS:` | Legacy reply / status lines |

---

## Request lifecycle (model)

```text
REQUEST_RECEIVED
  → REQUEST_ACCEPTED | REQUEST_REJECTED
  → ACTION_STARTED
  → ACTION_COMPLETED | ACTION_FAILED
  → STATE_CHANGED   (authoritative; Show Engine)
```

**Acceptance does not imply completion.**

`ShowduinoRequestContext` (`requestId`, `sourceDeviceId`, `targetDeviceId`, `messageType`, `sequence`) exists in the application model for a **future major version**. It is **not** serialized on v1 desk/node ESP-NOW layouts. Logical device-ID routing is **not** implemented yet — do not claim it.

Authoritative state comes from the Show Engine (**planned** structured snapshots; today mostly ACK/STATUS text lines).

---

## Validation

`protocol/showduino_validation.h` — pure C, no hardware deps.

Results include: `VALID`, `INVALID_MAGIC`, `UNSUPPORTED_VERSION`, `INVALID_SIZE`, `PAYLOAD_TOO_LONG`, `PAYLOAD_NOT_TERMINATED`, `EMPTY_PAYLOAD`, `INVALID_MESSAGE`, `INVALID_RELAY_CHANNEL`, `INVALID_RELAY_STATE`, `INVALID_NODE_TYPE`.

Active firmware Stage 2 keeps historical accept/reject behaviour on the live path (force NUL on last byte; silent truncate via `substring` on TX). Shared validators are available for new code and host tests.

---

## Unsupported / placeholder behaviour

Catalog meanings exist for `COMMAND_UNSUPPORTED`, `NOT_IMPLEMENTED`, `NODE_UNAVAILABLE`.

**Current runtime (unchanged in Stage 2):** PIXEL/AUDIO `ROUTE:` paths may still receive optimistic `ACK:` replies from the Communications Engine. Converting those to honest failure / unsupported replies is a **later stage** task — changing it now would alter live behaviour.

---

## Deprecated commands

| Command | Status |
|---------|--------|
| `RELAY:n:TOGGLE` | Deprecated; prefer absolute `ON`/`OFF`. Still parsed/executed by relay node. |

---

## Planned later (not implemented)

* Device-ID routing on the wire  
* Desk major **and** minor both encoded on wire  
* Node packet magic / stronger framing  
* UART CRC framing  
* Structured `STATE_SNAPSHOT` publication  
* ACK-driven Director UI (Stage 3+)  
* Removing live `TOGGLE` behaviour  
* Binary application framing of the message catalog  

---

## Arduino inclusion

Single source of truth: repo `protocol/`.

Active sketches (`firmware/<project>/<Sketch>/`) use:

```cpp
#include "../../../protocol/showduino_desk_packet.h"
```

Optional: junction/copy `protocol/` into Arduino `libraries/` using `library.properties` (`ShowduinoProtocol`), then `#include <showduino_desk_packet.h>`.

**Do not** copy headers into each firmware tree.

---

## Host tests

```text
tools/protocol-tests/
```

See that folder’s README. No ESP32 hardware required.

---

## Related files

* `protocol/README.md` — package overview  
* `docs/constitution.md` — roles and authority  
* `docs/architecture.md` — system topology  
