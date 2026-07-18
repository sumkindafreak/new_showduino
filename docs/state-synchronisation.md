# Showduino State Synchronisation (Stage 3)

**Status:** Implemented in Stage 3 for the currently supported runtime surface (show start/stop flags, emergency, relay channels, relay-node availability).

## Ownership

| Concern | Owner |
|---------|--------|
| Authoritative show / emergency / relay knowledge | Show Engine |
| Transport | Communications Engine |
| Display + pending UI | Director |
| Physical GPIO | Relay Node |

Acceptance ≠ completion. Confirmed relay display uses `STATE:RELAY` only.

## Relay lifecycle

```text
Director  RELAY:n:ON|OFF
   → Show Engine validates
   → ACCEPTED:RELAY:<seq>:n:ON|OFF   (forwarded; not completed)
   → ROUTE:RELAY:RELAY:n:ON|OFF
   → Node OK:RELAY:n:ON|OFF
   → STATE:RELAY:n:ON|OFF            (Director confirms UI)
```

Failures: `REJECTED:RELAY:…` (before route) or `FAILED:RELAY:…` (timeout / route / node error). Last confirmed state is retained.

Pending timeout: **3 s**. Relay node offline after **10 s** without node traffic.

## Snapshot / reconnect

On Director link READY:

1. `STATUS:REQUEST`
2. `SNAPSHOT:BEGIN` … lines … `SNAPSHOT:END`
3. Director exits SYNCING

Director restart does **not** stop a running show. Relays start as UNKNOWN until snapshot/node confirm.

## Emergency

- `STATE:EMERGENCY:ACTIVE|CLEAR` is authoritative.
- E-STOP may show local activating feedback immediately.
- E-CLEAR unlock waits for `STATE:EMERGENCY:CLEAR`.
- Legacy `STATUS:EMERGENCY_*` still emitted for compatibility.

## Placeholders

`PIXEL:*` / `AUDIO:*` → `UNSUPPORTED:…` (or `NODE_UNAVAILABLE:` if a leftover `ROUTE:` reaches C3). No false `ACK`.

## Not in Stage 3

Timeline engine, pause/resume, device-ID routing, UART CRC, binary framing, multi-node discovery.

See also: `docs/command-protocol.md`, `protocol/README.md`.
