# Showduino MOSFET Node 0.1.1

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
| Firmware | `0.1.1` |
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

Four WS2812 / NeoPixel LEDs on **GPIO25** visually mirror the four MOSFET outputs.

| Pixel | Maps to |
|-------|---------|
| 0 | OUT1 |
| 1 | OUT2 |
| 2 | OUT3 |
| 3 | OUT4 |

**NOT** a Showduino theatrical Pixel Line. Not a Pixel Node. Not Studio/SHDO Pixel authoring. Not a separate ESP-NOW peer or logical identity.

Expected wiring:

```text
GPIO25 → 330 Ω series → DIN Pixel 0 → Pixel 1 → Pixel 2 → Pixel 3
Common ground required. Suitable 5 V pixel supply (do not assume unlimited onboard 5 V).
```

Behaviour:

- OUT level 0% → indicator OFF  
- OUT level > 0% → green, brightness scaled from actual engine level (cap 64)  
- FADE / PULSE follow real output duty in real time  
- ALL OFF / Emergency / authority loss → all four OFF  
- `MOSFET:IDENTIFY` → ~5 s white chase on the four pixels; **MOSFET outputs unchanged**

Adafruit_NeoPixel (`NEO_GRB` + `NEO_KHZ800`). Fail-soft: if allocation/begin fails, MOSFET outputs still operate normally.

## Fail-safe

ALL OFF on: boot, reset, power restore, no P4 ownership, ownership loss, comms loss, fault, emergency, show stop/complete, production unload. Recovery never restores stale ON/PWM/pulse/fade — fresh authorised command only. Identifier pixels follow the same ALL OFF rule.

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
