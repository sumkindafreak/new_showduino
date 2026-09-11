# Showduino V1 physical-test checklist

Product: **1.0.0-rc.1**. Do not tag `v1.0.0` until this list is signed off.

Pass/fail must come from hardware. Source inspection is not a pass.

Dated gate record: [physical-acceptance-1.0.0-rc.1-2026-09-11.md](release/physical-acceptance-1.0.0-rc.1-2026-09-11.md). That pass found **no attached Showduino hardware** on the validation host. Every physical item remains **BLOCKED** / **NOT RUN**.

## Bench setup

1. P4 with SD containing at least `system_test`.
2. Comms S3 UART: S3 GPIO17 TX → P4 GPIO4 RX; S3 GPIO18 RX ← P4 GPIO5 TX; 115200 8N1.
3. Director ESP32-S3.
4. Audio Node (if present).
5. Lamp Node (if present).
6. Pixel Node (if present) — ESP32-C3 Super Mini OLED, DATA GPIO2, OLED GPIO5/6.
7. Phone/laptop on SoftAP `Showduino` → `http://192.168.4.1/` and `/studio/`. Pixel Node AP is `Showduino-Pixel-XXXX` at `http://192.168.5.1/` (commissioning only).

## Radio / power-cycle (release-blocking)

- [ ] Power P4 → Comms → Director. Director links without forcing channel 1 after Comms STA join.
- [ ] **Audio Node power-on while Director is linked: Director stays linked (10×).**
- [ ] Audio Node power-cycle 10× with show idle: no Director drop, no ESP-NOW deinit loop.
- [ ] Lamp Node join does not drop Director.
- [ ] Comms STA connect to a venue AP on a non-1 channel: Director and Audio follow without Comms deleting peers.
- [ ] STA disconnect: after ~3 s radio returns to channel 1; Director relinks.
- [ ] SoftAP `Showduino` remains joinable during STA connect.

## Boot / atmosphere

- [ ] Director boot 10×: first complete boot frame, then backlight; no fade-in of boot art.
- [ ] Boot sound and atmospheric LEDs coordinate; emergency white still overrides.

## Emergency / safety (must not regress)

- [ ] Physical E-stop GPIO25 latches; outputs safe; auto-resume does not happen.
- [ ] Director dual-action clear only after P4 clear request.
- [ ] GPIO23 emergency pixels all white; GPIO24 groups of 10; Lamp emergency white; Pixel Node entire configured line bright white.
- [ ] SHDO persist commit during emergency aborts; running show stops.

## Pixel Node (NEEDS HARDWARE TEST)

- [ ] Flash identical C3 firmware. Boot dark / `NOT INITIALISED`. OLED visible crop/orientation correct.
- [ ] Commission Node ID + friendly name. Save Count does not light. Initialise Line matches configured length.
- [ ] Segments persist across reboot. Independent simultaneous FX. Same vocabulary as P4 GPIO23.
- [ ] Locate is time-limited and loses to emergency. Webpage cannot clear emergency.
- [ ] P4 / Director / Studio see the node. Disconnect: timeline continues. Reconnect: rediscovery.
- [ ] Two nodes (`LED-01`, `LED-02`) independent. Pixel join does not drop Director/Audio/Lamp (10× power-cycle).

## Studio / persist

- [ ] `/studio/` RAM timeline PIXEL + AUDIO:NODE still works; START waits for P4.
- [ ] WebUI Productions persist of a minimal SHDO writes SD `manifest.json` + `timeline.json` and does **not** auto-load or auto-start.
- [ ] Director can PRODUCTION:LOAD the persisted id, then START.

## Network / updates

- [ ] WebUI Network shows AP / home Wi-Fi / internet / ESP-NOW channel separately from P4 Ethernet.
- [ ] Connect/disconnect/forget do not log passwords.
- [ ] Internet unplug does **not** show SHOWDUINO CONNECTION LOST on Director.
- [ ] Check for Updates returns `no_releases` or a newer tag without installing.

## Honesty

Any unchecked radio/Audio item is **NEEDS HARDWARE TEST**. Audio power-on vs Director remains a **BLOCKER** for tagging 1.0.0 until the 10× test passes.
