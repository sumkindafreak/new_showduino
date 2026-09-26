# Showduino MOSFET Node 0.1.0

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
| Firmware | `0.1.0` |
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

`SHOWDUINO_MOSFET_GPIO_VERIFIED` remains **0** until Toby electrically commissions his board.

PCB silk marks **5–60 V DC** input. Do not invent channel/board amperage, thermal, inductive, or mains ratings. Not a mains switching product. Commission with benign low-voltage test loads only.

## Fail-safe

ALL OFF on: boot, reset, power restore, no P4 ownership, ownership loss, comms loss, fault, emergency, show stop/complete, production unload. Recovery never restores stale ON/PWM/pulse/fade — fresh authorised command only.

## Commands (node-local)

```text
MOSFET:OUT:1:OFF
MOSFET:OUT:1:ON
MOSFET:OUT:1:LEVEL:50
MOSFET:OUT:1:PULSE:100:500
MOSFET:OUT:1:FADE:75:2000
MOSFET:ALL:OFF
MOSFET:OWN:GRANT
MOSFET:STATUS
```

P4 application form: `MOSFET:NODE:MOSFET-01:OUT:1:LEVEL:50`  
Comms route: `ROUTE:MOSFET:MOSFET-01:<seq>:<cmd>`

## Programming

UART programming via exposed header (IO0 / GND / RX / TX / 5V). Hold IO0 low during reset to enter download mode. Do not assume USB-C provides native USB serial. Avoid conflicting power sources.

## Physical commissioning checklist

See mission brief / `docs/node-roadmap.md`. Do not set `SHOWDUINO_MOSFET_GPIO_VERIFIED` to 1 until all four channels, LED polarity, boot/reset glitch behaviour, authority-loss ALL OFF, and Emergency ALL OFF are proven on the physical board.

Sketch: `ShowduinoMosfetNode/ShowduinoMosfetNode.ino`
