# Future LAN-device foundation

Status: **documented only**. Not implemented in Showduino 1.0.0-rc.1.

Showduino may later address ordinary LAN devices (for example a paludarium
controller, a network relay, or an HTTP actuator) as **logical show targets**,
not as a second show engine.

## Intended shape

A future production action such as `NETWORK_HTTP` would:

- be authored as intent (`target`, `url`, `method`, payload);
- be validated and dispatched by the **P4**;
- be transported by Comms only if the P4 cannot reach the LAN itself;
- never run from the Director;
- never bypass emergency/safety policy;
- never be required for the current Audio / Lamp / pixel path.

## V1 rules

- Do not add a `NETWORK_HTTP` cue compiler, WebUI editor, or P4 dispatcher yet.
- Do not advertise paludarium or third-party LAN integrations as shipping.
- Do not route household IoT traffic through ESP-NOW.
- P4 Ethernet remains an optional isolated show LAN (E1.31 observation only).
- Home/venue Wi-Fi on Comms is for WebUI convenience, update discovery, and
  future LAN reachability — not for show authority.

When this is implemented, it must be checked against Director, Comms, P4,
WebUI/Studio, protocol, and every applicable node (see the cross-component
synchronisation rule in `docs/constitution.md`).
