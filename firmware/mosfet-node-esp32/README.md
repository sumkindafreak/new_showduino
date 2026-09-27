# Showduino MOSFET Node 0.1.2

```text
Status: SOFTWARE IMPLEMENTED
        HARDWARE PIN MAP DEFINED
        PHYSICAL OUTPUT VALIDATION REQUIRED
Role: Specialist digital / PWM powered-output node
Board: ESP32_MOS_X4 / 303E32NMOS4
Supersedes: Relay Node product concept (Relay remains legacy/reference only)
```

## Board

| Item | Value |
|------|--------|
| PCB | ESP32_MOS_X4 / 303E32NMOS4 |
| MCU | ESP32-WROOM family (Arduino ESP32 Dev Module) |
| Firmware | `0.1.2` |
| Outputs | OUT1–OUT4 |
| SoftAP | `Showduino-MOSFET-01` @ `192.168.5.1` / password `showduino` |

### Software pin map (HARDWARE UNVERIFIED)

| Channel | GPIO |
|---------|------|
| OUT1 | 16 |
| OUT2 | 17 |
| OUT3 | 26 |
| OUT4 | 27 |
| Status LED | 23 |
| Identifier NeoPixels (×4) | 25 |

`SHOWDUINO_MOSFET_GPIO_VERIFIED` remains **0** until Toby electrically commissions his board.  
`SHOWDUINO_MOSFET_IDENTIFIER_PIXEL_VERIFIED` remains **0** until the GPIO25 chain is benched.

PCB silk marks **5–60 V DC** input. Do not invent channel/board amperage, thermal, inductive, or mains ratings. Not a mains switching product. Commission with benign low-voltage test loads only.

## Identifier NeoPixels (local only)

Four WS2812 / NeoPixel LEDs on **GPIO25** do double duty: primarily channel indicators, secondarily a dim four-position status display when that channel is OFF.

| Pixel | Primary (OUT active) | Idle status role (OUT off) |
|-------|----------------------|----------------------------|
| 0 | OUT1 green @ duty | Wi-Fi SoftAP — purple |
| 1 | OUT2 green @ duty | ESP-NOW — cyan (amber chase while searching) |
| 2 | OUT3 green @ duty | P4 ownership — turquoise |
| 3 | OUT4 green @ duty | Node health — violet |

**NOT** a Showduino theatrical Pixel Line. Not a Pixel Node. Not Studio/SHDO Pixel authoring. Not a separate ESP-NOW peer or logical identity.

Expected wiring:

```text
GPIO25 → 330 Ω series → DIN Pixel 0 → Pixel 1 → Pixel 2 → Pixel 3
Common ground required. Suitable 5 V pixel supply (do not assume unlimited onboard 5 V).
```

### Colour language

| Colour | Meaning |
|--------|---------|
| GREEN | Physical MOSFET output energised (brightness tracks 0–100% duty, cap 64) |
| PURPLE | Wi-Fi SoftAP up |
| MAGENTA | WebUI client activity / fault half |
| CYAN | ESP-NOW linked |
| TURQUOISE | P4 ownership granted |
| AMBER | Searching / degraded |
| VIOLET | Local healthy / boot |
| WHITE | IDENTIFY chase |
| BLACK / OFF | Safe / Emergency / fail-safe |

**Absolute rules:** green is never used for Wi-Fi, health, ESP-NOW, or ownership. Red is not part of the normal status vocabulary (Emergency = all LEDs OFF, matching MOSFET outputs OFF).

Idle status brightness is **8–12 / 255** (default 10) so a healthy box is not a Christmas tree. Channel state always wins over that pixel’s diagnostic role.

### Priority (highest first)

1. Emergency / fail-safe → ALL OFF  
2. Fault → alternating magenta ↔ amber  
3. IDENTIFY → white chase ~5 s (outputs unchanged)  
4. Active MOSFET output → green @ actual duty  
5. Node status / Wi-Fi / link → odd-colour indication on OFF pixels  

### Transient animations

| Event | Behaviour |
|-------|-----------|
| BOOT | Violet 1 → 2 → 3 → 4 once |
| ESP-NOW searching | Slow amber chase on OFF pixels |
| P4 grant received | Quick turquoise sweep 1 → 2 → 3 → 4, then normal |
| WebUI client connected | Magenta → OFF → Magenta on free pixels, then normal |
| IDENTIFY | White chase continuously for ~5 s |

Adafruit_NeoPixel (`NEO_GRB` + `NEO_KHZ800`). Fail-soft: if allocation/begin fails, MOSFET outputs still operate normally.

## Fail-safe

ALL OFF on: boot, reset, power restore, no P4 ownership, ownership loss, comms loss, fault, emergency, show stop/complete, production unload. Recovery never restores stale ON/PWM/pulse/fade — fresh authorised command only. Identifier pixels follow the same ALL OFF rule under Emergency / authority fail-safe.

## Commands (node-local)

```text
MOSFET:OUT:1:OFF
MOSFET:OUT:1:ON
MOSFET:OUT:1:LEVEL:50
MOSFET:OUT:1:PULSE:100:500
MOSFET:OUT:1:FADE:75:2000
MOSFET:ALL:OFF
MOSFET:IDENTIFY
MOSFET:OWN:GRANT
MOSFET:STATUS
```

P4 application form: `MOSFET:NODE:MOSFET-01:OUT:1:LEVEL:50`  
Identify: `MOSFET:NODE:MOSFET-01:IDENTIFY`  
Comms route: `ROUTE:MOSFET:MOSFET-01:<seq>:<cmd>`

## Programming

UART programming via exposed header (IO0 / GND / RX / TX / 5V). Hold IO0 low during reset to enter download mode. Do not assume USB-C provides native USB serial. Avoid conflicting power sources.

## Physical commissioning checklist

See `docs/physical-test-checklist.md`. Do not set verification flags to 1 until the board is electrically proven.

Sketch: `ShowduinoMosfetNode/ShowduinoMosfetNode.ino`


Status/identifier colours follow [`docs/status-colour-standard.md`](../../docs/status-colour-standard.md). Emergency keeps MOSFET power outputs OFF while the four GPIO25 identifier pixels show solid full-bright white.
