# Showduino State Synchronisation

**Status:** show/emergency synchronisation is implemented. Audio Node lifecycle tracking is active. P4 local pixel commissioning control is active. Relay fields remain legacy compatibility surfaces and are not the current Node roadmap.

## Ownership

| Concern | Owner |
|---------|-------|
| Authoritative show/emergency/runtime state | P4 Show Engine |
| Audio Node requested/confirmed lifecycle | P4 Show Engine, based on Node reports |
| P4 local pixel engine/segments | P4 Show Engine |
| ESP-NOW/UART transport | S3 Communications Engine |
| Browser static UI host | S3 Communications Engine |
| Operator display/pending UI | Director / Studio clients |
| Physical specialist-node hardware | The relevant Node |

**Acceptance ≠ completion.** A transmitted command is not proof that a remote physical action completed.

## Snapshot / reconnect

On Director link READY:

1. `STATUS:REQUEST`
2. `SNAPSHOT:BEGIN` … authoritative state lines … `SNAPSHOT:END`
3. Director exits SYNCING

Director restart or browser disconnect does **not** stop a running show.

## Emergency

- `STATE:EMERGENCY:ACTIVE|CLEAR` is authoritative.
- E-STOP may show local activating feedback immediately.
- E-CLEAR unlock waits for authoritative clear state.
- Legacy `STATUS:EMERGENCY_*` strings may still be emitted for compatibility.
- Emergency clear does not automatically resume a show or interrupted pixel effects.

### Global pixel safety state

Emergency has a fixed global pixel policy:

> **ALL PIXELS BRIGHT WHITE.**

The P4 GPIO23 Show Pixel Line and GPIO24 emergency/signage line implement the local side of this policy. Future C3 Pixel/Lantern pixel outputs and every later pixel-capable Node must implement the same override.

No Studio command, segment FX or production cue may override emergency white.

## Audio Node lifecycle

The specialist Audio Node reports accepted/started/completed/failed and state information through the Communications S3 to the P4. The P4 tracks this lifecycle rather than treating route/transmit as playback completion.

Programme audio remains Audio-Node-only. P4 ES8311 audio is system/safety audio only.

## P4 local pixel state

`PIXEL:*` is **not a generic unsupported placeholder anymore** for the P4 local line.

Current direct/commissioning commands include:

```text
PIXEL:STATUS
PIXEL:TEST
PIXEL:TEST:STOP
PIXEL:BLACKOUT
PIXEL:SOLID:r:g:b
PIXEL:BRIGHTNESS:<0-255>
PIXEL:SEGMENT:<id>:...
```

Segments support independent range/effect/colour/brightness/speed/intensity/randomness/reverse/duration state. The P4 remains authoritative for acceptance and emergency rejection.

Persistent production `PIXEL` cue parsing and named logical segment persistence are **not** implemented in production-format v1 yet. Direct `PIXEL:*` control should therefore be described as commissioning/runtime control, not completed production authoring.

Distributed C3 Pixel Node routing is also still future.

## Emergency/signage line state

GPIO24 is safety-owned and not a normal Studio production lane.

Normal state per 10-pixel sign group:

```text
GREEN + 9 OFF
```

Emergency state:

```text
10 WHITE
```

All configured sign groups are updated in one frame so the signage changes together.

## Legacy relay state

Relay lifecycle/state strings remain in compatibility code and historical documentation. The old Relay Node is superseded by the future MOSFET Node direction; do not treat Relay as the next production Node.

Where legacy relay state is still used, absolute ON/OFF remains preferred over distributed TOGGLE.

## Current specialist-node order

```text
Audio Node
→ C3 Lamp Node
→ C3 Pixel Node
→ MOSFET Node
```

DMX remains parked/out of scope.

## Still incomplete

- persistent production AUDIO/PIXEL cue types and dispatch;
- named logical pixel segment persistence/binding;
- logical device-ID routing end to end;
- generic completion-driven state/fault lifecycle for all future Nodes;
- structured binary framing beyond the current compatibility protocol;
- hardware commissioning of the current P4 pixel engine and Audio Node.

See also: `docs/command-protocol.md`, `docs/studio-pixel-authoring.md`, `protocol/README.md`.
