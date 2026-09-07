# P4 Ethernet + E1.31 test foundation

Milestone: prove the Waveshare ESP32-P4 can join an isolated wired show LAN, expose live/saved network state, receive valid E1.31/sACN, and keep running if that network disappears.

This is **not** the production E1.31 engine. Received values do not start productions, change outputs, or become authoritative show state.

## Transports

```text
Director ESP32-S3
        │ ESP-NOW   (Showduino control fabric)
        ▼
Dedicated ESP32-S3 Communications Engine
        │ UART      (Comms ↔ P4)
        ▼
ESP32-P4 Show Engine ── Ethernet ── show LAN
                              ├── WebUI / API
                              ├── E1.31 / sACN test RX
                              └── lighting equipment
```

Internet is not required and is not a health test.

## Hardware discovered

Board: Waveshare ESP32-P4-Module-DEV-KIT  
PHY: IP101GRI via RMII  
Arduino-ESP32 3.3.11: `ETH_PHY_TLK110` / `ETH_PHY_IP101`  
Generic FQBN `esp32p4` already matches the Waveshare Ethernet pin map.

| Signal | GPIO |
|--------|------|
| TX_EN | 49 |
| TXD0 | 34 |
| TXD1 | 35 |
| RXD0 | 29 |
| RXD1 | 30 |
| CRS_DV | 28 |
| REF_CLK | 50 (50 MHz from PHY, `EMAC_CLK_EXT_IN`) |
| MDC | 31 |
| MDIO | 52 |
| RESET | 51 |
| PHY address | 1 |

No conflict with UART 4/5, plugin I2C 7/8, PCM5102A 20–22, emergency pixels 24, E-stop 25, SDMMC 39–45, reserved C6 6/14–19/54, or RTC 0/1.

Pre-existing (not Ethernet): sketch `STATUS_LED_PIN` is GPIO10, which is onboard ES8311 LRCK.

## Pixel baseline

```text
P4
├── Emergency NeoPixel line (GPIO24)
└── General Show Pixel Line ×1 (planned GPIO23, not implemented)
Future expansion
└── Pixel / LED Nodes
```

Current source only implements emergency pixels. `PIXEL:` still replies `UNSUPPORTED:PIXEL`.

## Network configuration

`/showduino/config/network.json` on existing StageStorage/SD.

```json
{
  "formatVersion": 1,
  "ethernet": {
    "enabled": true,
    "mode": "DHCP",
    "ip": "",
    "subnet": "",
    "gateway": "",
    "dns": ""
  },
  "e131": {
    "enabled": true,
    "universe": 1
  }
}
```

Malformed files are rejected atomically. The P4 keeps defaults and boots.

## Failure behaviour

| Event | Result |
|-------|--------|
| Ethernet unplugged during show | Show continues. Ethernet OFFLINE. E1.31 UNAVAILABLE. |
| DHCP server disappears | Show continues. Last address may linger until lease/link loss. |
| E1.31 source disappears | Receiver STALE then OFFLINE. Local runtime continues. No cue action. |
| Malformed UDP | Rejected. Buffer not overflowed. |
| Heavy E1.31 | At most 8 packets per loop. UART, E-stop, timeline, audio, plugin bus, WebUI keep running. |

## Future Director page (not implemented)

Operator view only. P4 remains authoritative. Director does not configure Ethernet or universes.

- Ethernet status
- E1.31 status
- active universes
- source presence
- RX/TX indicators
- blackout request
- diagnostics

Reserved wire tokens exist in `protocol/showduino_state_wire.h`:

```text
STATE:ETHERNET:ONLINE | OFFLINE
STATE:E131:ONLINE | STALE | OFFLINE | UNAVAILABLE
```

HELLO currently reports `ETHERNET:ONLINE|OFFLINE` and `E131:<state>` only. No Director navigation change.

## Future production E1.31 engine (not implemented)

### RX
- multiple universes
- source priority
- sequence handling
- source timeout
- mapping E1.31 channels to Showduino logical inputs

### TX
- multiple universes
- multicast/unicast
- priority
- sequence numbers
- 512-channel buffers
- scheduled frame output
- logical Showduino outputs mapped to E1.31 channels

## Hardware bench procedure

Minimum useful setup:

```text
P4 Ethernet
     │
     ▼
Ethernet switch/router
     │
     ├── Laptop
     └── optional E1.31 sender (QLC+, sACN view, lighting desk)
```

| Test | Expect |
|------|--------|
| A — No cable | P4 boots. Show Engine RUNNING. Ethernet OFFLINE. E1.31 UNAVAILABLE. |
| B — Cable connected | `[NET] Ethernet link UP` |
| C — DHCP | `[NET] DHCP address acquired` plus IP/mask/gateway once |
| D — WebUI | Open `http://<P4-IP>/` and/or Comms Studio Network page |
| E — E1.31 multicast Universe 1 | Source name, rate, changing channel grid |
| F — Stop sender | STALE then OFFLINE. Last frame not treated as live |
| G — Unplug Ethernet | Local show, UART, emergency remain operational |
| H — Reconnect | Link and DHCP recover without reboot if the stack allows |
| I — Comms simultaneously | Director ↔ Comms ↔ P4 heartbeat stays healthy |
| J — Emergency simultaneously | GPIO25 still latches immediately while E1.31 is flowing |

Do not flash from this milestone automatically. Use the current P4 FQBN after a USB compile PASS.
