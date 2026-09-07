# P4 Ethernet + E1.31 test foundation

Milestone: prove the Waveshare ESP32-P4 can join an isolated wired show LAN, expose live/saved network state, receive valid E1.31/sACN, and keep running if that network disappears.

This is **not** a production E1.31 engine. Received values do not start productions, change outputs, drive pixels, or become authoritative show state.

**Current policy:** DMX/E1.31 production work is parked/out of scope until explicitly revisited. This test receiver remains available only as isolated diagnostic/observation infrastructure.

## Transports

```text
Director ESP32-S3
        │ ESP-NOW
        ▼
Dedicated ESP32-S3 Communications Engine
        │ UART
        ▼
ESP32-P4 Show Engine ── optional Ethernet ── isolated show LAN
                                      └── E1.31 / sACN test RX only

Browser
   │ Wi-Fi
   ▼
Communications S3 SoftAP + Studio WebUI
   │ UART/API proxy
   ▼
P4 authoritative API/state
```

The canonical browser WebUI is hosted by the Communications S3, not by P4 Ethernet. P4 Ethernet may expose API/diagnostic services where implemented, but losing Ethernet must not stop a show.

Internet is not required and is not a health test.

## Hardware

Board: Waveshare ESP32-P4-Module-DEV-KIT  
PHY: IP101GRI via RMII  
Arduino-ESP32 3.3.11: `ETH_PHY_TLK110` / `ETH_PHY_IP101`

| Signal | GPIO |
|--------|------|
| TX_EN | 49 |
| TXD0 | 34 |
| TXD1 | 35 |
| RXD0 | 29 |
| RXD1 | 30 |
| CRS_DV | 28 |
| REF_CLK | 50 |
| MDC | 31 |
| MDIO | 52 |
| RESET | 51 |
| PHY address | 1 |

No conflict with the current Comms UART 4/5, shared I2C 7/8, onboard ES8311 I2S 9-13, GPIO23 Show Pixels, GPIO24 emergency/signage pixels, GPIO25 E-stop, SDMMC 39-45, reserved C6 6/14-19/54, or RTC 0/1.

The old external PCM5102A GPIO20-22 path is retired. GPIO10 is ES8311 LRCK and is **not** a status LED.

## Current pixel baseline — independent of E1.31

```text
P4
├── GPIO23 Main Show Pixel Line
│   └── segmented non-blocking FX engine
└── GPIO24 emergency/signage Pixel Line
    └── 10-pixel sign groups

Future specialist expansion
└── C3 Pixel Node
```

The P4 local pixel engine is implemented in firmware and requires hardware commissioning. Direct `PIXEL:*` / `PIXEL:SEGMENT:*` commissioning commands are live.

**E1.31 does not feed this pixel engine.** There is intentionally no E1.31→pixel mapping in the current product path.

Emergency remains globally authoritative:

> **ALL PIXELS BRIGHT WHITE.**

Network traffic cannot override that state.

## Network configuration

`/showduino/config/network.json` on P4 SD:

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

Malformed files are rejected atomically. The P4 keeps safe/default state and continues booting.

## Failure behavior

| Event | Result |
|-------|--------|
| Ethernet unplugged during show | Show continues. Ethernet OFFLINE. E1.31 unavailable. |
| DHCP server disappears | Show continues. |
| E1.31 source disappears | Receiver becomes stale/offline. No cue or output action. |
| Malformed UDP | Packet rejected. |
| Heavy E1.31 test traffic | Must not starve UART, E-stop, timeline, audio, pixels or plugin bus. |
| Emergency during E1.31 traffic | Emergency latch wins; every pixel-capable output goes white. |

## Operator surfaces

The S3-hosted Showduino Studio may display P4 Ethernet/E1.31 diagnostic state, but it must not imply that E1.31 currently drives show outputs.

Reserved/state tokens may include:

```text
STATE:ETHERNET:ONLINE | OFFLINE
STATE:E131:ONLINE | STALE | OFFLINE | UNAVAILABLE
```

HELLO/status may report Ethernet/E1.31 health as diagnostics. That health is not the show authority.

## Future production E1.31 ideas — PARKED

The following are deliberately **not current work**:

- multiple universes;
- source priority/arbitration;
- channel→logical-input mappings;
- E1.31 output/transmit;
- channel→pixel mappings;
- DMX/sACN Studio authoring;
- production cues that depend on E1.31.

Do not implement or prioritise these until the project explicitly un-parks DMX/E1.31 work.

## Hardware bench procedure for the existing test receiver

Minimum useful setup:

```text
P4 Ethernet
     │
     ▼
Ethernet switch/router
     │
     ├── Laptop
     └── optional E1.31 sender/viewer
```

| Test | Expect |
|------|--------|
| A — No cable | P4 boots and local runtime remains available. |
| B — Cable connected | Ethernet link reports UP. |
| C — DHCP | Address acquired when DHCP exists. |
| D — E1.31 test RX | Source/rate/channel observation changes only. |
| E — Stop sender | Receiver becomes stale/offline; no output changes. |
| F — Unplug Ethernet | Local show, UART and emergency remain operational. |
| G — Comms simultaneously | Director/S3/P4 control fabric remains healthy. |
| H — GPIO23 pixels simultaneously | Local segmented FX continue independently of E1.31. |
| I — Emergency simultaneously | GPIO23 + GPIO24 immediately obey global white safety policy. |

Do not describe a successful E1.31 receive test as completion of a production lighting engine.
