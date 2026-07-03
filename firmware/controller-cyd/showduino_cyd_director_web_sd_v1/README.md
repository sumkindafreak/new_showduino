# Showduino CYD Director WebUI SD Server v1

This firmware turns the CYD into the Showduino Director web server.

## What It Does

- Starts a WiFi access point called `Showduino`
- Mounts the CYD SD card
- Serves the WebUI from `/www/` on the CYD SD card
- Provides browser API endpoints
- Sends commands to the Mega Stage Engine over Serial2
- Shows connection details on the CYD screen

## Upload File

Open this in Arduino IDE:

```text
firmware/controller-cyd/showduino_cyd_director_web_sd_v1/showduino_cyd_director_web_sd_v1.ino
```

## Required Libraries

```text
WiFi
WebServer
ESPmDNS
SPI
SD
TFT_eSPI
XPT2046_Bitbang
```

Most ESP32 installs already include WiFi, WebServer, ESPmDNS, SPI, and SD.

## CYD to Mega Wiring

```text
CYD TX pin 1  -> Mega RX1 pin 19
CYD RX pin 3  -> Mega TX1 pin 18
CYD GND       -> Mega GND
```

Baud:

```text
115200
```

## CYD SD Card Layout

Create this structure on the CYD SD card:

```text
/www/
  index.html
  app.js
  style.css
  assets/

/scenes/
/shows/
/projects/
/assets/
/logs/
/settings.json
```

The firmware will try to create the folders if the SD card mounts.

## WebUI Location

Put your built WebUI files inside:

```text
/www/
```

The key file is:

```text
/www/index.html
```

When your phone connects to Showduino and opens the dashboard, the CYD serves that file.

## Connecting With Phone

1. Power Showduino.
2. Wait for the CYD screen.
3. Connect your phone WiFi to:

```text
Showduino
```

Password:

```text
showduino
```

4. Open browser:

```text
http://192.168.4.1
```

Or try:

```text
http://showduino.local
```

## API Endpoints

### Status

```text
GET /api/status
```

Returns CYD/Mega status.

### Send Command to Mega

```text
GET /api/command?cmd=HEARTBEAT
GET /api/command?cmd=SCENE:TEST
GET /api/command?cmd=EMERGENCY:STOP
```

### Deploy Scene Commands

```text
POST /api/deploy
```

Body should be plain text, one Mega command per line:

```text
SCENE:BEGIN:portal_intro:10000
CUE:0:AUDIO:PLAY:001
CUE:0:PIXEL:1:EFFECT:PULSE
SCENE:END
```

### List CYD Scene Files

```text
GET /api/scenes
```

Lists `.shdo` and `.json` files in:

```text
/scenes/
```

## Missing WebUI Behaviour

If `/www/index.html` is missing, the CYD still boots and displays a friendly WebUI missing page.

This lets you confirm the server is working before adding the real dashboard files.

## Tonight Test

Use this first from your phone browser:

```text
http://192.168.4.1/api/status
```

Then:

```text
http://192.168.4.1/api/command?cmd=HEARTBEAT
http://192.168.4.1/api/command?cmd=SCENE:TEST
http://192.168.4.1/api/command?cmd=EMERGENCY:STOP
```

If those work, the WebUI can control the Mega through the CYD.
