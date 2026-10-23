# Showduino C3 Emergency + Pixel Node

```text
Status: IMPLEMENTED / GPIO UNCONFIRMED / PIXEL CAPABILITY ADDED
Role: Specialist wireless emergency station (ASSERT ONLY) + full Show Pixel Line
Hardware: ESP32-C3 Super Mini OLED
Firmware: 0.3.0
Product: Showduino 1.0.0-rc.1
Protocol: 1.0
```

This is an **additional** distributed wireless Emergency Button station. It does **not** replace the P4 GPIO25 hardwired main Emergency path.

The same physical peer (`ESTOP-01` … `ESTOP-08`) also provides the **full Showduino Pixel Controller** on **GPIO2**. Pixel is a **capability** of the Emergency Node — not a fake `LED-xx` identity.

```text
P4 MAIN EMERGENCY BUTTON ------------+
                                    |
ESTOP-xx C3 --- ESP-NOW -> COMMS (relay only) -> P4 +-> GLOBAL EMERGENCY LATCH
   | GPIO4 momentary Emergency pushbutton
   | GPIO2 WS2812 Show Pixel Line (full FX engine)
```

## Absolute rule

An Emergency Node may **ASSERT**.

An Emergency Node must **never CLEAR**.

There is no `ESTOP:CLEAR`. Button release does not clear. WebUI does not clear. Reboot does not clear P4 emergency.

Emergency handling always has priority over theatrical Pixel processing.

## Station authentication (MISSION 001J)

**A station must be provisioned before the P4 will accept its release.** An
unprovisioned or pre-001J station can still latch the P4 (ASSERT), but once it has
asserted it blocks every Director clear until it is provisioned or dismissed on the
P4 USB console. Full design and threat model: `docs/emergency-station-auth-001J.md`.

* The P4 challenges stations (`CHAL`); the station answers with an HMAC-SHA256
  `AUTH` line reporting its button and latch. Only that answer can mark the station
  RELEASED and ONLINE. `ANNOUNCE` and `STATUS` are display only.
* The station accepts ACK and global-clear observation only as P4-signed `CMD` lines
  with a counter it has never seen (stored before use). Plain `ESTOP:ACK:LATCHED`,
  `ESTOP:GLOBAL:OBSERVED` and `EMERGENCY:CLEAR` from the radio are rejected.
* The S3 Comms Controller only relays; it holds no keys.

### Register (USB console, 115200 baud, button released, not latched)

```text
PROVISION NEW ESTOP-07
```

The station prints `EMERGENCY:KEY:SET:ESTOP-07:<64 hex>` and a fingerprint once.
Paste that line into the **P4 USB console**; the P4 prints the same fingerprint.
Then clear the terminal scroll-back. Other commands: `PROVISION SET ESTOP-07 <hex64>`,
`PROVISION STATUS`, `PROVISION CLEAR` (all refused while pressed or latched).

### Replace a station

Provision the replacement with the **same ID** and enter its new key on the P4
(`EMERGENCY:KEY:SET:...` replaces the old one). The old board stops verifying. During
an emergency the station keeps blocking until the replacement proves RELEASED.
Never reuse a retired key.

### Retire a station

P4: `EMERGENCY:KEY:REVOKE:ESTOP-07`, then (after checking the physical station)
`EMERGENCY:STATION:DISMISS:ESTOP-07`. C3: `PROVISION CLEAR`.

### Limits

* The key is stored in NVS **without flash encryption**: anyone holding the board can
  read it and impersonate this station. Revoke and re-key a lost or stolen station.
* The SoftAP page still uses the default password below. With 001J it can drive
  only Pixel commands, cannot change a provisioned ID and cannot ACK or clear.
* Not hardware tested. Not a certified safety device.

## Sketch

```text
firmware/c3-emergency-node/ShowduinoC3EmergencyNode/
```

Shared Pixel engine:

```text
firmware/shared-pixel/
```

Arduino FQBN:

```text
esp32:esp32:esp32c3:CDCOnBoot=cdc,FlashMode=dio
```

Do **not** mark GPIO verified until the momentary pushbutton on GPIO4 is physically confirmed.

## GPIO

| Pin | Function |
|-----|----------|
| GPIO2 | Show Pixel data (WS2812 / NeoPixel, GRB 800 kHz). Recommend **330 Ω** series resistor. |
| GPIO4 | Momentary Emergency pushbutton (`INPUT_PULLUP`, pressed LOW) |
| GPIO5 | OLED SDA |
| GPIO6 | OLED SCL |
| GPIO9 | Optional maintenance local reset (long-press). Never clears P4. |

## Pixel capability

- Same command model as `firmware/c3-pixel-node/`
- Up to **512** pixels (validate loop timing / Emergency responsiveness on hardware)
- All **25** Showduino FX, segments, COUNT/INIT, persistence, Locate, commissioning TEST
- Studio / SHDO route: `estop-node-pixels` → `ESTOP:NODE:PIXEL:<cmd>`
- P4 strips prefix and forwards `PIXEL:<cmd>` to the same ESTOP ESP-NOW peer

When Emergency is latched: entire configured line → **Emergency White**. After legitimate clear with button released: line goes **BLACK** and does **not** auto-resume prior FX.

## SoftAP

SSID pattern: `Showduino-EStop-<id>` — password `showduino`. Compact PIXELS page for COUNT/INIT/TEST/segments.
