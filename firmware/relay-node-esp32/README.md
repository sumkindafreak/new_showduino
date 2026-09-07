# Showduino Relay Node — ESP32

```text
Status: LEGACY / SUPERSEDED
Role: Historical relay-node prototype
Replacement: planned Showduino MOSFET Node
```

This source is retained as a reference only. It is **not** the next Showduino node and must not be presented as the current output-node direction.

The specialist-node rollout is now:

```text
1. Audio Node
2. C3 Lantern Node
3. C3 Pixel Node
4. MOSFET Node
```

The MOSFET Node replaces the old Relay Node product concept. Do not continue relay-node feature development unless the architecture is explicitly revisited.

Historical prototype path:

```text
firmware/relay-node-esp32/ShowduinoRelayNodeEsp32/
```

Historical behaviour in this retained source includes ESP-NOW transport, absolute relay ON/OFF/pulse, local emergency safe state, and status reporting. Those implementation details may be useful when building the later MOSFET Node, but they are not the active node contract.

See [`docs/node-roadmap.md`](../../docs/node-roadmap.md), [`docs/constitution.md`](../../docs/constitution.md), and [`docs/repository-status.md`](../../docs/repository-status.md).
