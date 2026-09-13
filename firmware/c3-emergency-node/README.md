# Showduino C3 Emergency Node

```text
Status: IMPLEMENTED / GPIO UNCONFIRMED / DO NOT FLASH YET
Role: Specialist wireless emergency station (ASSERT ONLY)
Hardware: ESP32-C3 (Super Mini class proposed)
Firmware: 0.1.0
Product: Showduino 1.0.0-rc.1
Protocol: 1.0
```

This is an **additional** distributed emergency station. It does **not** replace the P4 GPIO25 hardwired NC/E-stop path.

```text
HARDWIRED NC E-STOP -------------------+
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

Do **not** flash until the mushroom switch GPIO is physically confirmed.

## Proposed GPIO (UNCONFIRMED)

| Function | GPIO | Notes |
|----------|------|-------|
| NC emergency input | **4** | INPUT_PULLUP. Closed=LOW healthy. Open=HIGH emergency. |
| Local re-arm | **9** | BOOT. Long-press after global clear only. Never clears P4. |
| Status LED | **-1** | Optional. Not fitted in V1 software default. |
| Buzzer | **-1** | Optional. Not required for V1. |

Opening the local NC loop is an emergency whether the mushroom is pressed or the local conductor is broken. V1 does not claim wire-break versus press distinction.

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

`HEALTHY` = booted, NC input readable, no local fault.  
`LINKED` = Comms/P4 have a fresh announce/heartbeat for that logical ID.

Taking every station offline together removes all wireless E-stop coverage and floods the desk with safety-node faults.

## SoftAP

Commissioning only: `Showduino-EStop-<id>` / `showduino` at `192.168.5.1`.

Not required for emergency operation.

## Offline policy (V1)

An Emergency Node going offline is a **SAFETY NODE FAULT / warning**.

It does **not** automatically assert global emergency.

## Not a certified life-safety system

Wireless ESP-NOW does not replace hardwired E-stop, fire-alarm, evacuation, machinery-safety, or regulatory systems an installation requires.
