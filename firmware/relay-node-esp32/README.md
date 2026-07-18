# Showduino Relay Node — ESP32

```text
Status: ACTIVE
Role: Showduino Relay Node
```

Nodes act. Canonical active relay node firmware.

```text
this Node --ESP-NOW--> Communications Engine --UART--> Show Engine
```

## Constitution

> The Nodes act.

The Show Engine decides what should happen. This node switches local relays and reports results. Command **acceptance** (on the Show Engine) and **physical completion** (on this node) are separate lifecycle events.

## Sketch

```text
firmware/relay-node-esp32/ShowduinoRelayNodeEsp32/
```

## Behaviour today

- ESP‑NOW receive/reply with the Communications Engine
- Absolute relay ON / OFF / pulse / all-off
- Local emergency safe state
- Status reporting

Application-level addressing should use **logical Showduino device IDs**. Current bring-up may still key off MAC at the transport layer — that is a known follow-up, not the long-term application model.

Distributed `TOGGLE` is not the preferred application command; prefer absolute ON/OFF requested by the Show Engine.

See [`docs/constitution.md`](../../docs/constitution.md) and [`docs/repository-status.md`](../../docs/repository-status.md).
