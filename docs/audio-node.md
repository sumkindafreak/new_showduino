# Showduino Audio Node

**Status:** IMPLEMENTED / HARDWARE TEST REQUIRED  
**Firmware:** `0.4.0` · protocol `1.3`  
**Board:** Ai-Thinker ESP32-Audio-Kit **V2.2 A161** · ESP32-A1S · **ES8388**  
**Not** the AC101 A1S (often silkscreen 2379).

The Audio Node uses the same ownership model as other specialist nodes. See [`standalone-node-architecture.md`](standalone-node-architecture.md). Playback states (`IDLE` / `PLAYING` / …) stay separate from ownership (`SEARCHING` / `STANDALONE` / `SHOW_CONTROLLED`).

```text
Director / WebUI  →  request
P4 Show Engine    →  decides and routes
Comms S3          →  ESP-NOW transport only
Audio Node        →  local SD → ES8388 → speaker / headphone / line
```

Programme / attraction audio is **Audio-Node-only**.  
P4 onboard ES8311 remains **Showduino system / safety audio** and is never a fallback.

MAC is identity metadata, discovered at runtime. The development unit observed `70:4B:CA:86:B3:E8` at factory boot. That value is **not** hard-coded.

## 1. Verified A161 pin map

Sources: official V2.2 / ESP32-A1S specification, NuttX and ESP-ADF Audio-Kit maps, plus the physical factory board (classic ESP32 rev 3, 4 MB, DOUT 40 MHz). Bench confirmation is still required for PA, SD detect, HP detect, and KEY1 analogue input.

| Function | GPIO | Notes |
|----------|------|-------|
| I2C SDA | 33 | ES8388 |
| I2C SCL | 32 | ES8388 |
| ES8388 address | 0x10 | Probe fallback 0x11 |
| I2S MCLK | 0 | Boot strap / BOOT — not a key |
| I2S BCLK | 27 | |
| I2S LRCK / WS | 25 | |
| I2S DOUT (DAC) | 26 | ESP → ES8388 |
| I2S DIN (ADC) | 35 | ES8388 ADC; onboard MIC1/MIC2 |
| PA enable | 21 | HIGH = amplifier on |
| SD SCK | 14 | SPI, not 4-bit SDMMC |
| SD MISO | 2 | Strapping pin |
| SD MOSI | 15 | Strapping pin |
| SD CS | 13 | **Shares KEY2** |
| SD detect | 34 | LOW = present (plus filesystem probe) |
| HP detect | 39 | LOW = jack; used only if config `output` is `AUTO` |
| KEY1 | 36 | Input-only, no pull-up. Local PLAY/STOP |
| KEY2 | — | **Disabled** — GPIO13 is SD CS |
| KEY3 | 19 | Volume down. **LED5 shares this pin — not driven as LED** |
| KEY4 | 23 | Volume up (VSPI MOSI unused on this kit) |
| KEY5 | 18 | Previous test asset |
| KEY6 | 5 | Next test asset (strapping; used as input after boot) |
| KEY7 | none | BOOT is GPIO0/MCLK. RST is hardware-only |
| Status LED | 22 | LED4 only |

### Conflicts

- GPIO13 = KEY2 and SD CS → KEY2 disabled
- GPIO0 = MCLK + boot strap
- GPIO19 = KEY3 and LED5 → LED5 unused
- GPIO2 / 5 / 15 are strapping pins
- I2C is 32/33, not 21/22
- Do not invent extra card-detect or jack-detect GPIOs

### PSRAM

Do not assume PSRAM. Firmware is built with `PSRAM=disabled`. Boot prints runtime `ESP.getPsramSize()`. Until that reports a size on the physical board, treat PSRAM as **absent**.

## 2. Outputs and inputs

| Path | Control | Status |
|------|---------|--------|
| Onboard speaker / PA | ES8388 LOUT2/ROUT2 + GPIO21 | IMPLEMENT NOW |
| Headphone | ES8388 LOUT1/ROUT1, PA off | IMPLEMENT NOW |
| Line | both DAC outs, PA off | IMPLEMENT NOW |
| `AUTO` | GPIO39 LOW → headphone else speaker | READY — only if jack detect is proven on the bench |
| Onboard mics / ADC | I2S DIN GPIO35, ES8388 ADC LIN1/RIN1 | IMPLEMENT NOW — meter / trigger / 8 s diag WAV |

`output` in config: `SPEAKER` `HEADPHONE` `LINE` `AUTO`. Default `SPEAKER`. Jack state is never guessed unless `AUTO` is set.

## 3. SD asset engine

```text
/showduino/
├── config/audio-node.json
├── config/audio-input-cal.json   explicit calibration only
├── recordings/diag.wav           optional 8 s commissioning clip
├── audio/
│   ├── system-test.wav
│   ├── ambience/
│   ├── effects/
│   ├── dialogue/
│   ├── music/
│   ├── stingers/
│   ├── test/
│   └── index.json          optional; not required
└── diagnostics/
```

Commands use relative paths under `/showduino/audio/`. Traversal (`..`, `\`, `//`) is rejected. Inventory is paged (6 names, max 48) over ESP-NOW. SD removal during play stops safely (`NO_STORAGE`) and does not reboot-loop. Reinsert remounts and returns IDLE — **no auto-resume**.

Card detect uses GPIO34 when the pin agrees with a periodic `cardSize()` probe. If the pin is wrong on a given board, probing still unmounts a dead card.

## 4. Config

`/showduino/config/audio-node.json` is all-or-nothing. Corrupt JSON → defaults + `CONFIG_FAULT`. Volume saves are debounced 1.5 s.

```json
{
  "formatVersion": 1,
  "volume": 80,
  "startupVolume": 80,
  "output": "SPEAKER",
  "commsTimeoutMs": 5000,
  "fadeDefaultMs": 0,
  "duckVolume": 30,
  "soundInput": {
    "enabled": true,
    "mode": "LEVEL_TRANSIENT",
    "threshold": 70,
    "thresholdAboveNoiseFloor": 20,
    "minimumDurationMs": 80,
    "sustainedDurationMs": 1500,
    "quietDurationMs": 5000,
    "cooldownMs": 3000,
    "hysteresis": 10,
    "postPlaybackInhibitMs": 1000,
    "triggerWhilePlaying": false,
    "autoNoiseFloor": true
  }
}
```

Invalid `soundInput` rejects the whole file (CONFIG_FAULT, defaults). Runtime adaptive noise floor stays in RAM. Only `SOUND:CALIBRATE` writes `audio-input-cal.json`.

`commsTimeoutMs` allowed range: 1000–30000.

## 5. Playback

States: `OFFLINE` `BOOTING` `IDLE` `LOADING` `PLAYING` `LOOPING` `PAUSED` `STOPPING` `EMERGENCY` `FAULT` `NO_STORAGE`.

Supported now: PLAY, LOOP, STOP, PAUSE, RESUME, VOLUME, FADE IN/OUT, DUCK/UNDUCK, STATUS, INVENTORY, TEST.

- WAV 16-bit PCM, mono or stereo, 8 / 16 / 22.05 / 32 / **44.1** / **48** kHz is mandatory.
- **MP3 / Ogg / FLAC are PLANNED.** 4 MB flash and unproven PSRAM make a second decoder a stability risk. WAV must stay working.
- One decoder only. Priority (SFX > dialogue > ambience) **interrupts**; it does not mix.
- Ducking is master/programme gain, not a second stream.
- Fades are non-blocking. Fade-out complete reports `AUDIO:IDLE`, not file `COMPLETED`.
- LOOP seeks back to the PCM `data` chunk and refills immediately. Expected gap is about one 2 ms playback-task slice plus SD seek — **not** sample-accurate/gapless.
- Small-effect RAM/PSRAM preload is **PLANNED** until PSRAM is proven. Start latency is reported as `start_ms` on `AUDIO:STATUS`.
- Reboot never auto-resumes attraction audio.

```text
AUDIO:NODE:PLAY:<path>[:FADE=ms][:PRI=SFX|DIALOGUE|AMBIENCE]
AUDIO:NODE:LOOP:<path>[:FADE=ms]
AUDIO:NODE:STOP
AUDIO:NODE:STOP:FADE=1500
AUDIO:NODE:PAUSE
AUDIO:NODE:RESUME
AUDIO:NODE:VOLUME:<0-100>
AUDIO:NODE:DUCK
AUDIO:NODE:UNDUCK
AUDIO:NODE:INVENTORY[:page]
AUDIO:NODE:STATUS
AUDIO:NODE:TEST
AUDIO:NODE:OWN:GRANT
EMERGENCY:STOP
EMERGENCY:CLEAR
```

`AUDIO:NODE:OWN:GRANT` is sent by the P4 (via Comms). Hearing ESP-NOW is not ownership. STATUS may keep an existing grant; it must not create one.

Lifecycle: `AUDIO:ACCEPTED` `STARTED` `COMPLETED` `FAILED` plus `AUDIO:CAPS:` `AUDIO:META:` `AUDIO:INVENTORY:`.

Capabilities advertised only if implemented:

```text
WAV,PLAY,LOOP,STOP,VOL,PAUSE,RESUME,FADE,DUCK,SPK,HP,LINE,INV,MIC,RECORD,STANDALONE,OWN
```

## 5b. Sound input / trigger engine

Architecture:

```text
Onboard MIC1/MIC2 → ES8388 ADC → Audio Node (16 kHz detect)
        → ESP-NOW SOUND:TRIGGER / SOUND:STATUS
        → Comms S3 → P4 Show Engine (logical input only)
```

The Audio Node may detect sound. The P4 decides what happens next. No production, cue, relay, pixel, or PLAY is started from a microphone event.

| Topic | Implementation |
|-------|----------------|
| Hardware | A161 two analog mics on MIC1/MIC2, MBIAS, ES8388 ADC LIN1/RIN1, I2S DIN GPIO35. Not AC101. |
| Sample rate | 16 kHz stereo 16-bit for analysis. Playback keeps the asset rate (8–48 kHz). |
| Duplex | Default **PLAYBACK_ONLY**: capture yields I2S during attraction play, then resumes. ES8388 ADC+DAC can share one clock, but onboard speaker couples into the mics. |
| Level | Short-term RMS + peak, smoothed 0–100. **Not dB SPL.** |
| Noise floor | Explicit `SOUND:CALIBRATE` (default 4 s). Slow downward adapt only. No upward chase of a screaming room. |
| Triggers | LEVEL, TRANSIENT, SUSTAINED. QUIET off unless `quietEnabled`. Cooldown + hysteresis re-arm. |
| Self-trigger | `triggerWhilePlaying` default false. `postPlaybackInhibitMs` default 1000. |
| Emergency | Stops attraction audio, inhibits sound events, no cached fire after CLEAR. |
| Comms loss | Meter may continue. Trigger reporting suspended. No stale replay. |
| Recording | Optional `SOUND:RECORD:TEST` max 8 s to `/showduino/recordings/diag.wav`. Not required for triggers. |
| Local bench | `SOUND:LOCAL_TEST_TRIGGER` USB only may play `system-test.wav`. Never from P4/show path. |
| FFT | **PLANNED** — not added; 4 MB / no PSRAM headroom first. |
| Voice AI | Not implemented. |

Director Audio Node page and WebUI show INPUT LEVEL / NOISE FLOOR / TRIGGER / LAST EVENT. Controls route Director/WebUI → Comms → P4 → Audio Node.

P4 stores `AUDIO_NODE_SOUND_TRIGGER` with subtype LEVEL / TRANSIENT / SUSTAINED / QUIET as a logical input. Future scare/clap/quiet-room mappings are not implemented.

## 6. Emergency, ownership, and comms loss

- `EMERGENCY:STOP`: stop, close file, mute ES8388, PA off, cancel fade/duck, reject new attraction play. No auto-resume.
- `EMERGENCY:CLEAR` → IDLE (or stay FAULT / NO_STORAGE). Does not resume the previous file.
- P4 `emergency.wav` is independent system audio.
- Boot: safe idle → ESP-NOW → search ~8 s for **P4 GRANT**. Grant → `SHOW_CONTROLLED`. Else → `STANDALONE` + SoftAP `Showduino-Audio-XXXX` on channel 1.
- P4 GRANT loss (no GRANT keepalive for 8 s): STOP, MUTE, IDLE, then standalone WebUI. No auto-resume.
- While `SHOW_CONTROLLED`, local keys, USB theatrical commands, and the node WebUI cannot PLAY/STOP/VOLUME. Firmware rejects them (`SHOW_CONTROLLED`). Hiding UI is not sufficient.
- GPIO22 remains a **status-only** diagnostic WS2812. Never a programme pixel.

## 7. Local commissioning

| Key | Gesture | Action |
|-----|---------|--------|
| KEY1 | short | Local test PLAY/STOP |
| KEY1 | long | Stop if playing, else STATUS dump |
| KEY2 | — | Disabled (SD CS) |
| KEY3 | short | Volume −5 |
| KEY4 | short | Volume +5 |
| KEY5 | short | Previous test asset |
| KEY6 | short | Next test asset |
| KEY7 | — | Not present |

Local keys never override emergency. PLAY/PREV/NEXT/VOLUME never hijack a P4-owned session.

### GPIO22 status pixel (diagnostics only)

| State | Pattern |
|-------|---------|
| BOOTING | slow dim blue (~700 ms) |
| SEARCHING (no P4 GRANT) | amber blink (~350 ms) |
| STANDALONE | slow violet |
| SHOW_CONTROLLED + IDLE | steady green |
| Fresh ESP-NOW RX (P4-owned) | brief bright-green kick |
| PLAYING / LOOPING | cyan |
| LOADING / STOPPING | cyan blink (~180 ms) |
| PAUSED | slow purple blink (~900 ms) |
| NO_STORAGE | three short orange flashes, pause |
| FAULT | fast red blink (~110 ms) |
| EMERGENCY | bright white |

`LED:TEST` forces a 1.5 s rapid pattern. `RUN:TEST` is silent and does not blast the speaker. Audible test is `AUDIO:TEST` at the current volume.

USB: `HELP` `STATUS` `MAC` `STORAGE:STATUS` `ASSET:LIST` `AUDIO:*` `SOUND:*` `KEYS:STATUS` `LED:TEST` `CODEC:STATUS` `RUN:TEST`. No arbitrary filesystem writes. Path traversal rejected. `SOUND:MONITOR` is 10 Hz Serial only.

## 8. WebUI

Two UIs exist. Neither is the show engine.

**Showduino Studio / Comms SoftAP:** browser → Comms S3 WebUI → P4 → Comms → Audio Node. Used when the node is P4-owned.

**Node SoftAP (firmware 0.4.1):** `Showduino-Audio-XXXX` on ESP-NOW channel 1, password `showduino`, `http://192.168.5.1` (not 192.168.4.1 — that is Comms Studio). Tabs: STATUS, AUDIO, LIBRARY, SOUND INPUT, CONNECTION, DIAGNOSTICS, SETTINGS.

- STANDALONE: full local PLAY/LOOP/STOP/VOLUME.
- SHOW_CONTROLLED: status only; theatrical posts return `CONTROLLED BY SHOWDUINO`.
- Live PLAY is never saved as a boot state.

## 9. Arduino FQBN

Match the factory 4 MB / 40 MHz part. Do not enable PSRAM until runtime detection proves it.

Arduino-ESP32 3.3.11 exposes `FlashMode=qio` and `FlashMode=dio` only — there is no `dout` menu option. Compile with **dio** (factory silkscreen was DOUT; the core maps this as DIO).

```text
esp32:esp32:esp32:PSRAM=disabled,FlashSize=4M,PartitionScheme=min_spiffs,FlashMode=dio,FlashFreq=40
```

Arduino-ESP32 3.3.x. Libraries: `WiFi`, `esp_now`, `SD`, `SPI`, `Wire`, `ESP_I2S`, `WebServer`, `Preferences`, Adafruit NeoPixel. No MP3 framework. SoftAP stays on ESP-NOW channel 1.

## 10. Hardware acceptance

A. Boot without SD — alive, `NO_STORAGE`  
B. Boot with SD + `system-test.wav` — codec + storage ready  
C. Confirm `[AUDIO NODE]` revision / flash / PSRAM / ES8388 / SD / MAC / firmware lines  
D. Local KEY1 plays test / selected asset  
E. KEY3 / KEY4 volume, KEY5 / KEY6 select  
F. KEY2 does not disturb SD  
G. LED patterns match the table  
H. Comms discovers Audio Node; WebUI Devices shows ONLINE  
I. Remote PLAY: ACCEPTED → STARTED → COMPLETED  
J. LOOP until STOP; listen for restart gap  
K. FADE / DUCK / UNDUCK  
L. Missing file: `FILE_NOT_FOUND`  
M. Pull SD during play — `NO_STORAGE`, no crash  
N. Emergency during play — stop, no resume; P4 emergency WAV still independent  
O. Comms / GRANT loss during LOOP — stop, then standalone WebUI; no resume  
P. WebUI + Director + volume + STATUS during continuous WAV — emergency still instant  

Sound-input bench (after ADC path is confirmed):

A. Boot — ES8388 ADC enabled, SOUND:STATUS READY  
B. Live level moves with speech/clap  
C. SOUND:CALIBRATE learns a stable noise floor  
D. Quiet room baseline does not climb  
E. Clap → one TRANSIENT  
F. Sustained loudness → one SUSTAINED when mode includes it  
G. Quiet trigger only if quietEnabled  
H. Cooldown blocks repeats  
I. Re-arm after level drops  
J. Playback does not self-trigger (`triggerWhilePlaying=false`)  
K. Post-playback inhibit  
L. Emergency suppresses sound events  
M. Director meter updates  
N. Director CALIBRATE  
O. WebUI meter/config  
P. Comms loss does not queue stale triggers  
Q. Playback stays clean while the input engine runs  

## 11. Deliberately not implemented

Bluetooth speaker, internet radio, Spotify/Alexa/DLNA, OTA, cloud, MP3/Ogg/FLAC decode, simultaneous mix, sample-accurate gapless, PSRAM preload, speech/wake-word AI, FFT bands, automatic scare/cue mapping from microphone events, Relay / MOSFET / LED / DMX work.
