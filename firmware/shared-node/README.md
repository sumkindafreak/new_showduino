# Shared specialist-node helpers

Arduino does not compile files outside the sketch folder. Each node sketch includes these sources from `src/SharedNodeSupport.cpp`:

```text
firmware/shared-node/NodeSoftAp.cpp
firmware/shared-node/NodeConfig.cpp
```

- **NodeSoftAp** — AP+STA SoftAP on the ESP-NOW channel, WPA2, channel reassert.
- **NodeConfig** — NVS name and u8 defaults only. Never store live show / ON / PLAY state.
