# Showduino Mega v1 Weekend Hardware Stack

This is the first practical Mega executor test build for Showduino v1.

## Target

```text
Arduino Mega 2560
```

## Connected Hardware

```text
4x NeoPixel data lines
SD card reader
RTC module
Audio control hardware / PCM-style audio board plan
CYD controller over Serial1
```

## Required Arduino Libraries

Install these in Arduino IDE:

```text
Adafruit NeoPixel
RTClib by Adafruit
SD
SPI
Wire
```

`SD`, `SPI`, and `Wire` are usually included with Arduino.

## Mega Pinout

### CYD Serial Link

```text
Mega RX1 = pin 19  ← CYD TX
Mega TX1 = pin 18  → CYD RX
Mega GND           ↔ CYD GND
Baud: 115200
```

### Pixel Lines

```text
Pixel Line 1 = Mega pin 4
Pixel Line 2 = Mega pin 5
Pixel Line 3 = Mega pin 15
Pixel Line 4 = Mega pin 16
```

Each pixel data line should ideally have a small resistor in series near the first pixel.

Use a proper external 5V supply for larger pixel strips.

Ground must be common between Mega and pixel power supply.

### SD Card Reader

Mega hardware SPI:

```text
MISO = 50
MOSI = 51
SCK  = 52
CS   = 53
```

### RTC

Mega I2C:

```text
SDA = 20
SCL = 21
```

## Upload File

Open this sketch in Arduino IDE:

```text
firmware/executor-mega/showduino_mega_v1/showduino_mega_v1.ino
```

Board:

```text
Arduino Mega or Mega 2560
```

Serial Monitor:

```text
115200 baud
Newline line ending
```

## Test Commands

You can type these into Serial Monitor first before connecting the CYD.

### Basic Status

```text
HEARTBEAT
STATUS:REQUEST
SD:STATUS
RTC:STATUS
```

### Pixel Tests

```text
PIXEL:1:COLOR:255,0,0
PIXEL:2:COLOR:0,255,0
PIXEL:3:COLOR:0,0,255
PIXEL:4:COLOR:255,255,255
PIXEL:ALL:BLACKOUT
```

### Pixel Effects

```text
PIXEL:1:EFFECT:PULSE
PIXEL:2:EFFECT:FIRE
PIXEL:3:EFFECT:STROBE
PIXEL:ALL:BLACKOUT
```

### Audio Hooks

These currently log/acknowledge the request until the exact audio hardware interface is confirmed.

```text
AUDIO:PLAY:001
AUDIO:VOLUME:80
AUDIO:STATUS
AUDIO:STOP
```

### Scene Test

```text
SCENE:TEST
```

Expected behaviour:

```text
0s    Audio track 001 requested
0s    Line 1 red pulse
0s    Line 2 fire flicker
4s    Line 3 blue pulse
7s    Line 4 white strobe
9s    Pixel blackout + audio stop
```

### Emergency Stop

```text
EMERGENCY:STOP
```

Expected behaviour:

```text
Scene stops
Pixels blackout
Audio stop requested
Mega reports STATUS:EMERGENCY_ACTIVE
```

Clear it with:

```text
EMERGENCY:CLEAR
```

## Important Audio Note

A bare PCM5102A DAC normally expects I2S audio data.

Arduino Mega does not have ESP32-style native I2S audio output.

So the current firmware deliberately uses an audio abstraction layer:

```text
AUDIO:PLAY:001
AUDIO:STOP
AUDIO:VOLUME:80
```

Once the exact audio board is chosen, these functions can be mapped to real pins, serial commands, or whatever interface the audio board requires.

## Weekend Build Goal

By the end of the weekend, the aim is to prove:

```text
CYD talks to Mega
Mega replies reliably
4 pixel lines work
SD status reports
RTC status reports
Scene test runs
Emergency stop blackouts pixels
Audio command layer is ready for the chosen hardware
```
