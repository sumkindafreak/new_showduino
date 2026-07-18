# Showduino Protocol Package

Canonical, transport-independent protocol definitions for Showduino.

**Owned by:** architecture / shared protocol (not a single board firmware).  
**Consumers:** Director, Communications Engine, Show Engine, Nodes.

## Constitution

> The Show Engine decides.  
> The Communications Engine transports.  
> The Director commands and displays.  
> The Nodes act.

Application messages describe **intent**. ESP-NOW and UART are **transports**.

## Layout

```text
protocol/
├── README.md
├── library.properties          # optional Arduino library metadata
├── showduino_protocol_version.h
├── showduino_message_types.h   # enum catalog (intent IDs)
├── showduino_messages.h        # aliases / catalog include
├── showduino_desk_packet.h     # Director <-> Comms ESP-NOW (v1 wire)
├── showduino_node_packet.h     # Comms <-> Node ESP-NOW (v1 wire)
├── showduino_validation.h      # pure C/C++ validators
├── showduino_legacy_strings.h  # colon-text compatibility constants
└── showduino_state_wire.h      # Stage 3 STATE/SNAPSHOT/ACCEPTED tokens
```

No pins, Wi-Fi, ESP-NOW init, UART drivers, LVGL, or GPIO live here.

## Version policy

Defined in `showduino_protocol_version.h`:

| Symbol | Meaning |
|--------|---------|
| `SHOWDUINO_PROTOCOL_VERSION_MAJOR` | Breaking wire changes |
| `SHOWDUINO_PROTOCOL_VERSION_MINOR` | Backward-compatible additions |
| `SHOWDUINO_DESK_WIRE_VERSION` | Value written in desk packet `version` field (currently `1`) |

Rules:

* Major mismatch → **reject** packet.
* Minor is package metadata for protocol v1; the desk wire field remains a single `uint16_t` equal to major (`1`).
* Unknown major versions must be rejected.
* Reserved / unused fields must be zero-initialized.
* Validate magic, version, and size before reading payload.

## Packets (implemented now — wire compatible)

### Desk packet (Director ↔ Communications Engine)

| Field | Type | Notes |
|-------|------|-------|
| `magic` | `uint32_t` | `0x5348444F` (`SHDO`) |
| `version` | `uint16_t` | Wire value `1` |
| `sequence` | `uint16_t` | Sender sequence |
| `sentMillis` | `uint32_t` | `millis()` snapshot |
| `command` | `char[96]` | NUL-terminated legacy colon text |

Expected size: **108** bytes.

### Node packet (Communications Engine ↔ Node)

| Field | Type | Notes |
|-------|------|-------|
| `nodeType` | `char[16]` | e.g. `"RELAY"` (legacy category string) |
| `command` | `char[96]` | NUL-terminated legacy colon text |
| `sequence` | `uint32_t` | Sequence / echo id |

Expected size: **116** bytes.  
**No magic on the wire today** (v1 compatibility). Magic for node packets is **planned** for a future major version.

## Application message catalog

See `showduino_message_types.h`. Intent IDs such as `SHOW_START_REQUEST`, `RELAY_SET_REQUEST`, `EMERGENCY_ACTIVATE_REQUEST`.

**Implemented on wire today:** colon-delimited legacy strings (compatibility layer).  
**Catalog enums:** available for firmware and host tools; full structured binary framing is **planned**.

## Legacy UART / text envelopes (compatibility)

| String prefix / token | Role |
|----------------------|------|
| `SHOW:START` | → `SHOW_START_REQUEST` |
| `SHOW:STOP` | → `SHOW_STOP_REQUEST` |
| `EMERGENCY:STOP` | → `EMERGENCY_ACTIVATE_REQUEST` |
| `EMERGENCY:CLEAR` | → `EMERGENCY_CLEAR_REQUEST` |
| `RELAY:n:ON/OFF` | → `RELAY_SET_REQUEST` |
| `RELAY:n:TOGGLE` | **Deprecated** — still parsed by some firmware |
| `ROUTE:<type>:` | Legacy Comms envelope (Show Engine → Comms) |
| `NODE:` | Legacy Comms envelope (node reply to Show Engine) |
| `ACK:` / `ERR:` / `STATUS:` | Legacy reply lines |

These envelopes are **not** the permanent application vocabulary.

## Request lifecycle (model)

```text
REQUEST_RECEIVED → REQUEST_ACCEPTED|REJECTED
→ ACTION_STARTED → ACTION_COMPLETED|FAILED
→ STATE_CHANGED (authoritative, Show Engine)
```

Acceptance ≠ completion. Device-ID routing fields (`requestId`, `sourceDeviceId`, `targetDeviceId`) are in the **application model** for a future major version — **not** claimed on v1 wire.

## Arduino inclusion

All active sketches sit three levels below repo root. Prefer:

```cpp
#include "../../../protocol/showduino_desk_packet.h"
```

Optional: copy or junction this folder into the Arduino `libraries/` path using `library.properties` (`ShowduinoProtocol`), then `#include <showduino_desk_packet.h>`.

**Do not** fork copies of these headers into each firmware tree.

## Host tests

```text
tools/protocol-tests/
```

Compile without Arduino / ESP32 SDK (`run_tests.ps1` / `run_tests.sh`).

## Arduino inclusion strategy (Stage 2)

**Canonical source:** this `protocol/` directory (one copy only).

| Approach | Use when |
|----------|----------|
| Relative `#include "../../../protocol/..."` | Default for all four active sketches (sketch path is always `firmware/<project>/<Sketch>/`) |
| Arduino library install / junction of `protocol/` into `libraries/ShowduinoProtocol` | Optional IDE convenience; same files, no forks |

Do **not** duplicate headers under each firmware tree. PlatformIO/CMake are not required for Stage 2.

## Honest status

| Area | Status |
|------|--------|
| Shared desk/node structs | Implemented |
| Shared magic/version/max | Implemented |
| Validation helpers | Implemented (live RX gated in Stage 3) |
| Message type enum catalog | Implemented |
| Legacy string constants + mappers | Implemented |
| Authoritative STATE / snapshot wire | Implemented (Stage 3) |
| Director pending vs confirmed relay UI | Implemented (Stage 3) |
| Logical device ID on wire | Planned |
| Desk major/minor both on wire | Planned (minor currently metadata) |
| Node packet magic | Planned |
| Removing node-side TOGGLE execution | Later (Director no longer sends it) |
