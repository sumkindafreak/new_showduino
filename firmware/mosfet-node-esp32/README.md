# Showduino MOSFET Node

```text
Status: PLANNED — DO NOT IMPLEMENT YET
Role: Future specialist switched-output node
Supersedes: Relay Node product concept
```

The MOSFET Node is the planned switched-output node after the C3 Pixel Node.

Current rollout order:

```text
Audio Node → C3 Lamp Node → C3 Pixel Node → MOSFET Node
```

No firmware or hardware assumptions are locked here yet. The old relay prototype is retained under `firmware/relay-node-esp32/` as legacy/reference material only.

When this milestone is explicitly started, design it around the Showduino constitution: P4 decides, Comms transports, Nodes act and report confirmed state/completion.

See [`docs/node-roadmap.md`](../../docs/node-roadmap.md).
