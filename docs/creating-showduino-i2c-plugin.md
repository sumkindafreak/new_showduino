# Creating a Showduino I²C Plugin

Goal: add many simple I²C modules by placing JSON on the SD card, and add complex modules through a stable native driver without redesigning the Stage Engine.

Read [`docs/plugin-bus.md`](plugin-bus.md) first.

---

## 1. Electrical check

The Plug-in Bus is **3.3V I²C** on GPIO7 (SDA) and GPIO8 (SCL). If the module’s SDA/SCL are 5V, stop and add a bidirectional level shifter. Confirm the module address from its datasheet (not from a guess).

---

## 2. Prefer an SD definition

Copy a file into `/showduino/plugins/devices/` on the P4 SD card. Schema version is **1**.

Minimum:

```json
{
  "showduino_plugin_schema": 1,
  "id": "vendor.part",
  "name": "Human readable name",
  "transport": "i2c",
  "addresses": ["0x48-0x4F"],
  "driver": "generic.i2c.register",
  "capabilities": ["temperature"]
}
```

`addresses` is a hint for identify matching. **Presence at an address is not identity.** Overlapping addresses (PCA9685 0x40–0x7F, EEPROMs, muxes) stay unknown until:

- an `instances[]` entry in `registry.json` names that location, or
- `identify` succeeds uniquely.

### Safe identify (optional)

Only if the datasheet documents a **read-only** ID register that does not change device state:

```json
"identify": {
  "register": "0x0F",
  "mask": "0xFF",
  "equals": "0xBC"
}
```

Do not invent register probes.

### Name a physical unit

`/showduino/plugins/registry.json`:

```json
{
  "showduino_plugin_schema": 1,
  "instances": [
    {
      "instance": "crypt-door-servos",
      "device": "nxp.pca9685",
      "location": { "bus": 0, "address": "0x40" },
      "friendly_name": "Crypt Door Servos"
    }
  ]
}
```

That asserts *this installation’s* wiring. It still does not load a PCA9685 waveform driver until native `pca9685` firmware exists.

Templates: `firmware/stage-engine-p4/sd-overlay/showduino/plugins/`

---

## 3. When you need a native driver

Implement a `PluginDriver` in `src/plugin/PluginDrivers.cpp` (or a dedicated `.cpp`) when you need:

- PWM/servo timing
- GPIO expander direction/latches
- ToF ranging
- IMU fusion
- Emergency-safe output writes

Register it with `pluginDriverFind()`. Do not load compiled binaries from SD.

---

## 4. Multiplexer

If two modules share a fixed address, put a TCA9548A on the bus and set location:

```json
"location": { "bus": 0, "mux": 112, "channel": 3, "address": "0x40" }
```

(`112` is `0x70`.) Assign device `nxp.tca9548a` so firmware will scan mux channels.

---

## 5. Verify

USB console, 115200:

```text
PLUGIN:SCAN
PLUGIN:INFO:0x40
PLUGIN:STATUS
```

Unknown ACKs are expected and safe.

---

## 6. What SD must never do

Plugin JSON cannot:

- execute code
- reassign GPIO25, GPIO24, the Communications Engine UART, SDMMC, or reserved C6/SDIO pins
- bypass emergency latch
- allocate unbounded buffers or polling loops
