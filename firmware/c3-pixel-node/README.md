# Showduino C3 Pixel Node

```text
Status: IMPLEMENTED / NEEDS HARDWARE TEST
Role: Specialist Show Pixel Line (remote equivalent of P4 GPIO23)
Hardware: ESP32-C3 Super Mini OLED (HUNT-proven pin map)
Firmware: 0.1.0
Product: Showduino 1.0.0-rc.1
```

This is a **specialist output node**, not a lighting console and not a second Show Engine.

```text
Director ESP32-S3
        │ ESP-NOW
        ▼
ESP32-S3 Communications Engine
        │ UART
        ▼
ESP32-P4 Show Engine
        │ ROUTE:PIXEL:<id>:
        ▼
this C3 Pixel Node  (ESP-NOW, local FX render)
```

Flash the **same firmware** onto every Pixel Node. Commission logical identity (`LED-01`, friendly name, line length) after flashing.

## Sketch

```text
firmware/c3-pixel-node/ShowduinoC3PixelNode/
```

Arduino FQBN:

```text
esp32:esp32:esp32c3:CDCOnBoot=cdc,FlashMode=dio
```

```text
arduino-cli compile --fqbn "esp32:esp32:esp32c3:CDCOnBoot=cdc,FlashMode=dio" firmware/c3-pixel-node/ShowduinoC3PixelNode
```

Do not flash unless explicitly instructed.

## Pinout (locked)

| Function | GPIO | Notes |
|----------|------|-------|
| OLED SDA | **5** | I²C 400 kHz |
| OLED SCL | **6** | I²C 400 kHz |
| OLED address | 0x3C | SSD1306 (`OLED_DRIVER_SH1106 false`) |
| OLED rotation | 180° | Visible glass crop: L28 R4 T24 H38 |
| Button A | 9 | BOOT / local test |
| Button B | 0 | OLED page |
| **Pixel DATA** | **2** | WS2812/NeoPixel DIN. Lamp-proven on this board. |

Do **not** move GPIO5/GPIO6. Do **not** drive HUNT GPIOs 3, 4, 7, 8, 10.

## Electrical

The C3 drives **DATA only**. Do not power a substantial pixel line from the Super Mini 5 V/3V3 pins.

```text
ESP32-C3 GPIO2 ── 330–470 Ω ── WS2812 DIN
ESP32-C3 GND   ─────────────── pixel GND ── PSU GND
External 5 V PSU ──────────── pixel 5 V
```

Recommended:

* series data resistor **330–470 Ω** close to the first pixel DIN;
* bulk capacitor **1000 µF** (or similar) across pixel 5 V/GND at the strip start;
* common ground between C3 and pixel PSU;
* 3.3 V → 5 V **logic-level buffer** on DATA for long leads or noisy 5 V strips;
* fuse the pixel PSU for the expected strip current;
* inject 5 V every ~1–2 m / ~60–100 pixels on dense 5 V WS2812 lines.

Current (rule of thumb, WS2812 full white ≈ 60 mA/pixel):

| Pixels | Worst-case 5 V |
|--------|----------------|
| 60 | ~3.6 A |
| 100 | ~6 A |
| 300 | ~18 A |
| 512 (C3 max) | ~30.7 A |

Always size the PSU, wiring, injection and fuse for the actual density and brightness limit. Theatrical looks rarely sit at full white; commission with a conservative brightness cap.

C3 maximum is **512** pixels (frame time + RAM + ESP-NOW). P4 GPIO23 remains **1–1024**. Same Showduino model; hardware max differs.

## Commissioning

1. Boot: `PIXELS: 0`, `LINE: NOT INITIALISED`, DATA held low.
2. Open SoftAP `Showduino-Pixel-XXXX` → `http://192.168.5.1/` (not Comms `192.168.4.1`).
3. Set Node ID (`LED-01`) and friendly name.
4. Set Line Pixels → **Save Count** (must not light the strip).
5. **Initialise Line**.
6. Configure segments / tests. Webpage is not required to run a show.

## Communication loss

If P4 ownership is lost: **blackout** (`SHOWDUINO_FAILSAFE_PIXEL_BLACKOUT`). Stale FX are not resumed. OLED reports loss. Other nodes and the P4 timeline continue.

## Emergency

Authoritative emergency: **entire configured line bright white**. Segments, FX, Locate and WebUI tests are ignored. Webpage cannot clear system emergency.

## Locate

`PIXEL:LOCATE` — ~4 s magenta chase + OLED `LOCATE`. Below emergency. Then blackout until P4 resync.
