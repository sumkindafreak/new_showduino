# Showduino C3 Emergency Node

```text
Status: IMPLEMENTED / GPIO UNCONFIRMED / OLED ADDED
Role: Specialist wireless emergency station (ASSERT ONLY)
Hardware: ESP32-C3 Super Mini OLED
Firmware: 0.2.0
Product: Showduino 1.0.0-rc.1
Protocol: 1.0
```

This is an **additional** distributed wireless Emergency Button station. It does **not** replace the P4 GPIO25 hardwired main Emergency path.

```text
P4 MAIN EMERGENCY BUTTON ------------+
                                    |
ESTOP-xx C3 --- ESP-NOW -> COMMS -> P4 +-> GLOBAL EMERGENCY LATCH
```

## Absolute rule

An Emergency Node may **ASSERT**.

An Emergency Node must **never CLEAR**.

There is no `ESTOP:CLEAR`. Button release does not clear. WebUI does not clear. Reboot does not clear P4 emergency.

## Sketch

```text
firmware/c3-emergency-node/ShowduinoC3EmergencyNode/
```

Arduino FQBN:

```text
esp32:esp32:esp32c3:CDCOnBoot=cdc,FlashMode=dio
```

Do **not** mark GPIO verified until the momentary pushbutton on GPIO4 is physically confirmed.

## Intended GPIO (button UNVERIFIED)

| Function | GPIO | Notes |
|----------|------|-------|
| Momentary emergency pushbutton | **4** | INPUT_PULLUP. RELEASED=HIGH. PRESSED=LOW → assert/latch. |
| OLED SDA | **5** | Same as C3 Pixel Node |
| OLED SCL | **6** | Same as C3 Pixel Node |
| Maintenance local reset | **9** | BOOT long-press. Optional. Never clears P4. Not required for normal use. |
| Status LED | **-1** | Optional. Not fitted in V1 software default. |
| Buzzer | **-1** | Optional. Not required for V1. |

OLED: SSD1306 0x3C, 128×64, 400 kHz, 180° rotation, Pixel Node visible viewport.

Press asserts and latches. Release never clears. After an authoritative P4 clear with the button released, the station returns to READY automatically. If the button is still held during that clear, the station remains latched and re-asserts.

## Sequential update / commissioning

Never flash or reboot two Emergency Nodes at the same time.

```text
ESTOP-01 update
        ↓
      reboot
        ↓
 healthy + linked
        ↓
ESTOP-02 update
        ↓
 healthy + linked
        ↓
ESTOP-03 ...
```

`HEALTHY` = booted, button readable, no local fault.  
`LINKED` = Comms/P4 have a fresh announce/heartbeat for that logical ID.

Taking every station offline together removes all wireless emergency-button coverage and floods the desk with safety-node faults.

## SoftAP

Commissioning only: `Showduino-EStop-<id>` / `showduino` at `192.168.5.1`.

Not required for emergency operation.

## Host tests

```text
powershell -File tools/emergency-node-tests/run_tests.ps1
```
