# Showduino CYD Director v1 Test Firmware

This is the first CYD controller firmware for testing the Showduino v1 Director → Stage Engine architecture.

## Target

```text
ESP32-2432S028R / CYD 2.8 inch touchscreen
```

## Purpose

The CYD is the Director.

It sends commands to the Mega Stage Engine over UART serial.

The Mega runs the hardware.

## Upload File

Open this in Arduino IDE:

```text
firmware/controller-cyd/showduino_cyd_director_v1/showduino_cyd_director_v1.ino
```

## Required Libraries

```text
TFT_eSPI
XPT2046_Bitbang
```

Your TFT_eSPI setup must already match your CYD display.

## Wiring to Mega

```text
CYD TX pin 1  -> Mega RX1 pin 19
CYD RX pin 3  -> Mega TX1 pin 18
CYD GND       -> Mega GND
```

Baud:

```text
115200
```

## Matching Mega Firmware

Upload this to the Mega:

```text
firmware/executor-mega/showduino_mega_v1/showduino_mega_v1.ino
```

## CYD Buttons

The test screen includes:

```text
HEARTBEAT
STATUS
SCENE TEST
SCENE STOP
PIXEL BLACKOUT
AUDIO 001
P1 Pulse
P2 Fire
P3 Strobe
P4 Blue Solid
EMERGENCY STOP
```

## Tonight Test Flow

1. Upload Mega firmware.
2. Open Mega Serial Monitor at 115200.
3. Confirm Mega reports READY.
4. Upload CYD firmware.
5. Wire CYD to Mega Serial1.
6. CYD should send HEARTBEAT.
7. Mega should reply STATUS:ALIVE.
8. Press SCENE TEST.
9. Confirm pixels run the test sequence.
10. Press EMERGENCY STOP.
11. Confirm pixels blackout.

## If Touch Is Wrong

If touch feels rotated or mirrored, adjust this part in the sketch:

```cpp
x = map(p.x, 200, 3900, 0, 320);
y = map(p.y, 200, 3900, 0, 240);
```

Some CYD screens need X/Y swapped or inverted depending on the panel.

## What This Is Not Yet

This is not the finished scene creator.

This is the first working controller panel so you can run the system tonight.

The next step is to add:

```text
CYD SD scene storage
Scene list browser
Scene deploy to Mega
Mega runtime scene playback
WebUI scene creator
```
