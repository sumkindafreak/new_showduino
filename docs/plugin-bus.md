# Showduino Plug-in Bus

Permanent I²C peripheral infrastructure for the ESP32-P4 Stage Engine.

This bus discovers electrically compatible I²C devices, represents unknown devices safely, lets simple register devices be described on SD, and lets complex devices attach through a native driver registry. It does **not** claim to support every I²C module.

Director → ESP-NOW → ESP32-S3 Comms Controller → UART GPIO4/5 → P4 is unchanged. The Plug-in Bus is an addition.

---

## Electrical interface

| Signal | Function |
|--------|----------|
| **3.3V** | Module logic power (Showduino Plug-in Bus power rail) |
| **GND** | Common ground |
| **SDA** | I²C data |
| **SCL** | I²C clock |

### Pin assignment (Waveshare ESP32-P4-Module-DEV-KIT)

| Signal | P4 GPIO | Where it appears |
|--------|---------|------------------|
| SDA | **GPIO7** | Dedicated SH1.0 **I2C** header; 40-pin header pin 3 (Pi-style SDA) |
| SCL | **GPIO8** | Dedicated SH1.0 **I2C** header; 40-pin header pin 5 (Pi-style SCL) |

Sources: Waveshare [ESP32-P4-Module-DEV-KIT](https://www.waveshare.com/wiki/ESP32-P4-Module-DEV-KIT-StartPage) (`SDA(GPIO7)`, `SCL(GPIO8)`), schematic nets `ESP_I2C_SDA` / `ESP_I2C_SCL`, [IDF examples](https://docs.waveshare.com/ESP32-P4-Module-DEV-KIT/Development-Environment-Setup-IDF).

These pins are shared with:

- Onboard **ES8311** audio codec (typical address **0x18**)
- MIPI CSI / DSI control/touch I²C

Showduino show audio remains the **PCM5102A** on GPIO20/21/22. The ES8311 is discovered as an onboard plugin and is not used as the show DAC.

Do not use GPIO4/5, 6, 14–19, 20–22, 24, 25, 39–45, or 54.

### Voltage

- ESP32-P4 GPIOs are **3.3V** only.
- Pull-ups must be to **3.3V**.
- Maximum safe bus voltage on SDA/SCL: **3.3V**.
- Never pull SDA/SCL to 5V.
- 5V-powered modules require a **bidirectional I²C level shifter**. Do not wire 5V module SDA/SCL directly to the P4.

### Pull-ups

Waveshare documents onboard I²C pull-ups on this net (`enable_internal_pullup = false` in their IDF example). Schematic pull-ups on `ESP_I2C_*` are to **ESP_3V3** (including 2.2 kΩ parts near the I²C/codec area and 10 kΩ nearby).

For short bench cables, board pull-ups are enough. For longer cables, add **2.2 kΩ–4.7 kΩ to 3.3V** at the far end only if the bus is sluggish — do not stack many extra pull-ups.

Internal ESP32 pull-ups are weak and are **not** relied on.

### Bus length and capacitance

I²C is not an industrial hot-plug fieldbus. Keep the Showduino Plug-in Bus short (aim under ~50 cm of cable for 100 kHz). Extra capacitance from long wiring, many modules, and level shifters slows edges. If ACK becomes unreliable, shorten the cable or reduce device count before raising speed.

### Power arrangement

Showduino exposes **3.3V + GND + SDA + SCL** as the plugin connector. 5V may power *isolated* module logic only through a level-shifted I²C boundary. 12V prop power is never on the I²C connector.

---

## Bus speed

Default: **100 kHz** standard-mode.

Per-device 400 kHz is allowed in a plugin definition for documentation, but the shared bus does not automatically switch to 400 kHz because that can break slower neighbours. A future per-mux-channel or second physical bus may use 400 kHz explicitly.

---

## Software architecture

| Term | Meaning |
|------|---------|
| **Bus** | Physical I²C transport (`plugin-bus-0` today) |
| **Device** | Something that ACKs at an address |
| **Driver** | Native firmware that understands a protocol |
| **Plugin definition** | SD JSON describing identity/config/capabilities |
| **Instance** | One attached device, named and located |

Location is **not** address alone:

```text
bus0/0x40
bus0/mux70/ch3/0x40
```

Firmware:

```text
src/plugin/PluginTypes.h
src/plugin/PluginBus.h/.cpp
src/plugin/PluginRegistry.h/.cpp
src/plugin/PluginDriver.h
src/plugin/PluginDrivers.cpp
src/plugin/PluginJson.h/.cpp
```

Future cue addressing (stable internal API, production format unchanged):

```text
pluginBusFindByInstanceId("crypt-door-servos")
pluginBusFindByLocation(...)
```

Conceptual target path: `plugin:crypt-door-servos/channel/2`

---

## Discovery

At boot, after emergency, SD, audio, and C6 UART:

1. Sample SDA/SCL. If stuck LOW, warn and continue. Bounded SCL clocking is attempted only if SDA is stuck and SCL is free.
2. `Wire.begin` at 100 kHz with a 50 ms timeout.
3. Load SD registry/definitions if present.
4. Scan 7-bit addresses **0x08–0x77**. No writes. No register probing unless a definition declared a safe `identify` block.
5. Match: SD instance map → safe identify → onboard ES8311 at 0x18 → else `generic.i2c.unknown`.

Unknown devices are not errors:

```text
[PLUGIN] 0x37 detected — UNKNOWN I2C DEVICE
```

Address overlap (for example 0x40) is **never** assumed to be a PCA9685 without instance config or a declared identify match. Multiple identify hits → `AMBIGUOUS`.

If no devices ACK:

```text
[PLUGIN] Plug-in Bus ready — no devices detected
```

The Stage Engine still reaches `[SYSTEM] Showduino ready`.

---

## SD layout

```text
/showduino/plugins/registry.json
/showduino/plugins/devices/*.json
```

SD is optional. Without it:

```text
[PLUGIN] SD plugin registry unavailable — native discovery only
```

Malformed JSON is skipped; other files still load. Schema field `showduino_plugin_schema` must be **1**. Duplicate definition/instance IDs are reported.

Example files to copy: `firmware/stage-engine-p4/sd-overlay/showduino/plugins/`

SD holds data only. It cannot execute native code, change GPIO25, C6 UART, SDMMC, or reserved C6/SDIO pins.

---

## Generic register driver

Driver id: `generic.i2c.register`

A definition may include `identify.register` / `mask` / `equals`. That **one** read happens only when the definition is present. Values (scale, endian, units) are reserved in the schema for a later poller; identification is implemented now.

---

## Native drivers (this generation)

| id | Role |
|----|------|
| `generic.i2c.unknown` | ACK seen, no safe identity |
| `generic.i2c.register` | SD-declared identify read |
| `waveshare.es8311` | Onboard codec at 0x18 (do not treat as show audio) |
| `tca9548a` | Mux: channel scan only when this driver is assigned |

Add complex devices (PCA9685, MCP23017, VL53L0X, …) as native drivers later. Do not bulk-add Arduino libraries.

---

## Multiplexers

TCA9548A-style muxes are part of the location model. Channel scans run only if a live instance is actually assigned the `tca9548a` driver (SD instance + definition). A random ACK at 0x70 is **not** treated as a mux.

---

## Capabilities

Plugins expose capability bits (`digital.output`, `pwm.output`, `temperature`, …), not brand names. See `PluginTypes.h`.

---

## Device loss

Registered devices are pinged one-at-a-time every 5 s. ONLINE→OFFLINE and OFFLINE→ONLINE log **once** per transition. The Stage Engine continues.

I²C is not robust hot-plug. Use `PLUGIN:SCAN` after wiring changes.

---

## Emergency

GPIO25 / GPIO24 / P4 latch remain authoritative.

`pluginBusOnEmergency()` walks output-capable instances and calls driver `onEmergency`. SD `safe_state` cannot override the P4 emergency system. Drivers that do not implement output-safe writes log nothing and must not block. Plugin failure must never stall E-stop.

---

## USB console

```text
PLUGIN:SCAN
PLUGIN:LIST
PLUGIN:STATUS
PLUGIN:INFO:<instance|0xNN>
```

Listed in `HELP`. Local USB replies stay on Serial; they are not a Director protocol requirement.

---

## RUN:TEST hook

`pluginBusCaptureSelfTest()` fills:

- bus init, SDA idle, SCL idle, scan
- devices detected / known / unknown / offline configured
- definition load PASS/FAIL

`RUN:TEST` itself is not implemented in this task.

---

## Future Showduino native I²C protocol

A future Showduino-designed peripheral may expose a small register map (manufacturer id, device id, firmware, capability bitmap, mailbox). Reserved for a later revision. Not implemented now.

---

## First module wiring

1. Power the module from **3.3V** (or 5V **only** behind a level shifter).
2. GND to P4 GND.
3. SDA → GPIO7 (I2C header SDA or 40-pin pin 3).
4. SCL → GPIO8 (I2C header SCL or 40-pin pin 5).
5. Confirm header orientation against the board silkscreen.
6. USB Serial `PLUGIN:SCAN`.
