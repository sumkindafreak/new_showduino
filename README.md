# Showduino

**Modular show control for scare attractions, escape rooms, immersive experiences and interactive props.**

**Current product baseline:** 1.0.0-rc.1 · **Protocol:** 1.0 · **Studio interchange:** SHDO v2  
**Status:** active development / integrated bench commissioning. This is **not** a certified or hardware-signed-off production release.

Showduino coordinates attraction audio, addressable lighting, interactive props, trigger inputs and operator controls using a central ESP32-P4 Show Engine and specialist ESP32 nodes. A loaded show runs on the Show Engine: it does not need an open browser, a connected Director touchscreen, home/venue Wi-Fi or internet access.

> **Architectural rule:** The Show Engine decides. The Communications Engine transports. The Director commands and displays. The Nodes act.

## At a glance

| Part | Hardware | Responsibility |
| --- | --- | --- |
| **Show Engine / Stage Controller** | Waveshare ESP32-P4 | Authoritative show runtime, cue scheduling, emergency policy, SD storage and local hardware |
| **Communications Engine** | Dedicated ESP32-S3 Dev Module | ESP-NOW transport, UART bridge, Showduino Wi-Fi access point, embedded WebUI and API proxy |
| **Director** | ESP32-S3 800 × 480 touchscreen | Operator interface, show requests, status, node controls and diagnostics |
| **Specialist nodes** | ESP32-A1S, ESP32-S3 and ESP32-C3 boards | Local audio, interactive lamp, remote pixels and wireless emergency assertion |

The Waveshare P4 board's onboard ESP32-C6 is **unused/reserved**. The older ESP32-C3/SUE communications controller and legacy relay product role are **not** the current architecture.

## System architecture

~~~text
Director ESP32-S3 touchscreen
          ↕ ESP-NOW
Dedicated ESP32-S3 Communications Engine ←→ Wi-Fi browser / local Studio
          ↕ UART, 115200 8N1
ESP32-P4 Show Engine / Stage Controller
          ├─ SD: productions, configuration, system audio and logs
          ├─ ES8311: local system / emergency audio
          ├─ GPIO23: segmented Show Pixel Line
          ├─ GPIO24: emergency / designated-signage pixel line
          ├─ GPIO25: physical momentary emergency button
          └─ I²C Plug-in Bus: expansion inputs / outputs

Audio, Lamp, Pixel and Emergency Nodes
          ↕ ESP-NOW through Communications S3
ESP32-P4 remains authoritative
~~~

The Comms S3 serves the browser frontend and forwards commands; it **does not** execute timelines, own emergency state or invent completion acknowledgements. The P4 can operate its already-loaded show independently when the browser or Director disconnects. A disconnected specialist node may still lose its individual effect; each node has its own defined fail-safe behavior.

**Comms wiring:** S3 TX GPIO17 → P4 RX GPIO4; S3 RX GPIO18 ← P4 TX GPIO5; common GND; 115200 8N1. See [hardware pinout](docs/hardware-pinout.md) before connecting hardware.

## What the current firmware supports

### Show Engine

- Start, pause, resume and stop a loaded timeline, with P4-owned show and emergency state.
- Discover, validate and load SD productions; accept a supported SHDO v2 deployment through the Comms/P4 compile-and-store path.
- Run a direct/commissioning RAM timeline for supported audio and pixel cues.
- Drive a local segmented Show Pixel Line on GPIO23; up to 1,024 configured pixels and 16 segment slots.
- Drive a separate GPIO24 emergency/designated-signage line; up to 100 configured pixels in ten-pixel sign groups.
- Play **system / emergency audio** through the onboard ES8311. Attraction/programme audio belongs to the Audio Node.
- Expose local diagnostics, SD storage, Plug-in Bus foundations and authoritative Web API state.

### Specialist-node status

**Source availability is not the same as a physically accepted installation.** Node behavior below describes implemented firmware and its intended hardware role; complete multi-node and safety acceptance remains a separate gate.

| Node | Intended capability | Repository status |
| --- | --- | --- |
| **Audio Node** (ESP32-A1S / ES8388) | SD-based WAV attraction audio; play, loop, stop, pause, volume, fade/duck, local sound-level/trigger diagnostics and standalone/managed ownership | Active firmware; physical audio, radio and end-to-end cue tests required |
| **S3 Lamp Node** | Interactive carbide-lamp simulation: striker button, seven-pixel flame Jewel, blow detection, local Fermion audio, motion provision and standalone or Showduino-managed operation | Active firmware; integrated physical acceptance required; optional motion input needs verification |
| **C3 Pixel Node** | One remote WS2812/NeoPixel line on GPIO2, segmented effects, OLED and node commissioning; configured limit 1–512 pixels | Active firmware; physical output and radio testing required |
| **C3 Emergency Node** | Additional wireless station that can **assert**, but never clear, the P4 emergency latch | Active firmware; station-specific hardware/input commissioning and safety acceptance required |
| **MOSFET Node** | Future digital switching and PWM/dimming specialist | **Planned; not a completed production node** |
| **DMX / E1.31 production control** | Possible future stage-lighting integration | **Parked expansion; not included as a supported production-control feature** |

The retired Relay Node and historical C3 Lamp implementation remain in the repository for reference; do not confuse them with current specialist products. See the [repository status](docs/repository-status.md) and [node roadmap](docs/node-roadmap.md). Older per-component README files may lag the active source and should not override current BoardConfig or implementation.

### Pixel lighting

The P4 GPIO23 line and remote C3 Pixel Nodes use the [shared effect vocabulary](protocol/showduino_pixel_fx.h): solid colour, fades, pulse/breathe, flicker/candle/fire, lightning/strobe, chase/bounce/comet/wipe, sparkle/twinkle/glitch, warning, portal, rainbow and other defined effects. Segments allow different effects on regions of the same physical strip.

GPIO24 is **not** another theatrical lighting lane. In normal operation, each ten-pixel designated-signage group has one green locator pixel and nine off. When Showduino emergency is asserted, the configured P4 show/signage lines and pixel-capable nodes are commanded bright white. On authorised clear, the signage line returns to its normal pattern and theatrical pixel effects **do not automatically resume**.

Configure pixel counts and initialise each line before testing output. Use appropriately rated external 5 V power, common grounds, signal conditioning/level shifting and suitable wiring/protection; see the [hardware pinout](docs/hardware-pinout.md) and [Pixel Node commissioning guide](firmware/c3-pixel-node/README.md).

## Studio, show files and deployment

Showduino has **two distinct browser surfaces**:

- The **Comms-hosted system console** provides status, configuration, production management, outputs and commissioning at the local Showduino address.
- **Studio V4** is the evolving attraction/scene/timeline authoring experience. It uses the **SHDO v2** interchange model. Its source and embedded snapshot must not be mistaken for a fully tested, unrestricted on-device editor.

The currently implemented workflow has important boundaries:

1. Studio/commissioning can send supported **PIXEL and AUDIO:NODE cues** to a P4 **RAM timeline** using \`/api/studio-timeline\`. This does not persist the show to SD and does not automatically start it.
2. A supported **SHDO v2** package can be uploaded through the Comms production-deploy API. The P4 validates and **compiles a supported subset** into its own SD runtime representation (\`manifest.json\` and \`timeline.json\`); it does **not** execute arbitrary \`.shdo\` JSON directly. Successful commit neither auto-loads nor auto-starts the production.
3. The stored/runtime cue representation and supported compiler actions are **not the same as unrestricted Studio authoring**. Unsupported actions or devices are rejected. In particular, do not assume a complete mixed-device attraction show is proven merely because its SHDO package exports.
4. After deployment, an operator loads the production and deliberately starts it through the Director or supported local control surface. The P4 then owns execution.

See [Studio architecture and boundaries](docs/studio/README.md), [SHDO v2 format](docs/studio/production-format.md), [production storage/deployment](docs/production-storage.md), and the [first Audio Node test production](examples/productions/README.md). Some older subproject documentation still calls all persistent production cues TEST/LOG-only; the current SHDO compiler/deploy code additionally handles a bounded subset of audio, pixel and lamp actions. Treat feature-specific host and hardware tests as authoritative for readiness.

## Getting started on the bench

> **For commissioning only.** This is not a final venue installation or a substitute for the physical acceptance checklist.

1. Follow the component-specific firmware instructions; use the **actual board type, flash/PSRAM configuration and live pinout** for each device. The overall release-candidate version does not imply every component has the same firmware version.
2. Prepare the P4 SD card and physically wire Comms ↔ P4 with the UART connections and common ground shown above.
3. Power the **P4**, then **Comms S3**, then **Director**. Confirm P4 storage, communications link and authoritative runtime status before operating effects.
4. Join the Comms Showduino access point using the configured **bench credentials**, then open **http://192.168.4.1/** for the system console or **http://192.168.4.1/studio/** for its embedded Studio snapshot. Home/venue Wi-Fi and internet are optional for local show execution.
5. Commission the intended node IDs, pixel counts, audio assets, inputs and output hardware. Start with safe low-power tests, then prove the supported deployment/load/start path and expected emergency behavior.

Individual nodes may provide a **separate commissioning SoftAP**, commonly at **http://192.168.5.1/**. That is **not** the main Comms console. Change the documented default bench Wi-Fi credential before public/venue use; do not assume home-Wi-Fi configuration changes the Showduino AP password.

Use the [quick start](docs/manual/SHOWDUINO_QUICK_START.md), [user manual draft](docs/manual/SHOWDUINO_USER_MANUAL.md) and [physical test checklist](docs/physical-test-checklist.md). Operator guidance remains subject to hardware acceptance and current UI verification.

## Emergency and safety boundaries

The P4's **main physical button** is a **momentary, active-LOW GPIO25 input**. A valid press latches Showduino emergency after input debounce; releasing the button does **not** clear it. Holding the **same continuous press for eight seconds** also requests Director Locate. Locate wakes/illuminates the Director and flashes its ambient LEDs until acknowledged; the first touchscreen press acknowledges **Locate only**, not the emergency latch.

The Director emergency-clear workflow requires an explicit request and confirmation after the physical input is released. New assertions invalidate a pending clear. Clearing emergency does **not** automatically restart a show. Wireless Emergency Nodes may assert the global latch but may **never** clear it. Their documented NC input arrangement is **different from the P4's momentary button**; do not copy one pin/polarity assumption to the other.

**Showduino is not a certified life-safety, fire-alarm, evacuation, machinery E-stop or safety-PLC system.** Wireless links and Showduino's theatrical emergency lighting are not replacements for independent, code-compliant emergency systems or a site-specific risk assessment. The effects and safety behavior in this repository must be physically accepted before venue use; emergency-latch persistence through complete power loss is **not** yet an established product guarantee.

See [wireless emergency requirements](docs/emergency-node.md), [hardware pinout](docs/hardware-pinout.md), [manual verification gaps](docs/manual/SHOWDUINO_MANUAL_GAPS.md) and [physical acceptance checklist](docs/physical-test-checklist.md).

## Firmware and repository map

| Path | Purpose |
| --- | --- |
| [\`firmware/stage-engine-p4/\`](firmware/stage-engine-p4/) | ESP32-P4 Show Engine; historical folder name retained |
| [\`firmware/s3-comms-controller/\`](firmware/s3-comms-controller/) | Dedicated Comms S3, WebUI hosting and transport |
| [\`firmware/director-esp32-8048s050/\`](firmware/director-esp32-8048s050/) | ESP32-S3 touchscreen Director |
| [\`firmware/audio-node-esp32-a1s/\`](firmware/audio-node-esp32-a1s/) | Specialist Audio Node |
| [\`firmware/s3-lamp-node/\`](firmware/s3-lamp-node/) | Current S3 interactive Lamp Node |
| [\`firmware/c3-pixel-node/\`](firmware/c3-pixel-node/) | Remote C3 Pixel Node |
| [\`firmware/c3-emergency-node/\`](firmware/c3-emergency-node/) | Wireless emergency station |
| [\`protocol/\`](protocol/) | Shared commands, packet schemas, SHDO, pixel effects and versions |
| [\`web/showduino-studio/\`](web/showduino-studio/) | Editable Comms-hosted console source |
| [\`web/studio-v4-overlay/\`](web/studio-v4-overlay/) | Studio V4 authoring/website integration source |
| [\`tools/\`](tools/) | Tests, WebUI embedding, release and development helpers |
| [\`docs/\`](docs/) | Architecture, hardware, operator and engineering documentation |

The Comms frontend is generated into \`firmware/s3-comms-controller/ShowduinoS3CommsController/src/web/WebAssets.generated.h\` from editable sources. Regenerate it with \`python tools/embed-webui/embed_webui.py\` after frontend changes; **do not manually edit the generated header**.

## Updates and release readiness

**Comms self-OTA** is implemented in software with HTTPS firmware download, SHA-256 comparison and a post-reboot health gate/rollback path. It is **Comms-only**; system-wide OTA and specialist-node OTA are not available. Installing a Comms update requires the documented maintenance, idle-show, emergency and operator-confirmation gates. Do not claim that source-code implementation proves the OTA/rollback scenario passed a live hardware test.

Showduino remains **1.0.0-rc.1** until integrated hardware sign-off. The release checklist covers Director/Comms radio stability, audio and specialist-node behavior, SD deployment, physical emergency/clear/Locate, pixel emergency override and recovery after faults. Firmware compilation and protocol host tests are useful evidence, **not** a substitute for those physical tests.

- [Architecture and ownership](docs/architecture.md)
- [Current repository classification](docs/repository-status.md)
- [Showduino V1 feature matrix](docs/v1-feature-matrix.md)
- [Comms OTA](docs/comms-ota.md)
- [Physical test checklist](docs/physical-test-checklist.md)
- [Commercial manual draft](docs/manual/SHOWDUINO_USER_MANUAL.md)

**Development note:** Some subproject READMEs, status tables and planned-feature documents are older than active firmware. When they disagree, check the current component's BoardConfig, implementation, protocol and test evidence before describing a feature as available or physically verified.
