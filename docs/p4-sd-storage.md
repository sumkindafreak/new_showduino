# P4 SD card — primary persistent storage

**SD is the persistent backbone, not the safety backbone.**

The ESP32-P4 onboard microSD (FAT32, SDMMC Slot 0) is the canonical Showduino
filesystem. Firmware defaults, UART pins/baud, emergency latch logic, and the
Communications S3 PROGMEM WebUI do not depend on it.

```text
FIRMWARE / PROGMEM   boot, recovery defaults, safety, minimum diagnostics
RAM                  live show, scheduler, protocol, time-critical buffers
SD CARD              config, productions, assets, logs, diagnostics, backups
```

Storage format version: **1** (`/showduino/system/storage-version.json`).
Incompatible versions are rejected whole-file. Firmware defaults remain active.

## Canonical tree

```text
/showduino/
├── productions/<id>/     manifest.json, timeline.json, assets/
├── config/
│   ├── system.json
│   ├── network.json
│   ├── e131.json
│   ├── plugin-bus.json
│   ├── pixels.json
│   ├── nodes.json        reserved; omit if empty
│   └── director.json
├── audio/                shared + emergency audio
│   └── system/
├── assets/               shared reusable files (not the Comms WebUI)
├── logs/                 system.log, comms.log, network.log, emergency.log, …
├── diagnostics/          last-test.txt, run-test-<session>.txt
├── backups/config-<session>/
├── import/               staging only; never auto-trusted
├── export/               generated packages
├── recovery/             previous known-good config copies
└── system/               storage-version.json, last-boot.json
```

## Ownership

| Path | Owner | Notes |
|---|---|---|
| `productions/` | P4 ProductionStore | RAM timeline after load |
| `config/system.json` | P4 | Last loaded ID, name, idle boot only |
| `config/network.json` | P4 ShowNetwork | Ethernet only. No live link state |
| `config/e131.json` | P4 E1.31 test RX | Universe / enable. No mappings |
| `config/plugin-bus.json` | P4 PluginBus | Roles. Presence is runtime |
| `config/pixels.json` | Reserved | One planned GPIO23 show line. Engine off |
| `config/director.json` | P4-owned prefs only | Director-local UI stays on the Director |
| `logs/` | P4 StageLog | Buffered, rotated, event-based |
| `webui/` | Legacy | Canonical UI is Comms S3 PROGMEM |
| `shows/` | Legacy | Do not delete user content |

## Path audit (current vs this pass)

| Existing path | Current use | Decision |
|---|---|---|
| `/showduino/` | Root | **Keep** |
| `/showduino/productions/` | Runtime productions | **Keep** |
| `/showduino/config/` | plugin-bus, network | **Keep** + new files |
| `/showduino/config/network.json` | Ethernet + legacy E1.31 | **Keep** ethernet; E1.31 **moved** to `e131.json` |
| `/showduino/config/plugin-bus.json` | Roles | **Keep** |
| `/showduino/audio/` | Parent of system audio | **Keep** |
| `/showduino/audio/system/` | P4 system/safety WAV set | **Keep** |
| `/showduino/diagnostics/` | `last-test.txt` | **Keep** + session export |
| `/showduino/plugins/` | Device JSON | **Keep** |
| `/showduino/logs/` | Created, unused | **Keep** as flat `*.log` |
| `/showduino/backups/` | Created, unused | **Keep** + `config-<session>/` |
| `/showduino/system/` | Created, unused | **Keep** + version / last-boot |
| `/showduino/shows/` | Legacy packages | **Legacy** — read-old, do not delete |
| `/showduino/webui/` | Old static UI | **Legacy** — not required |
| `/showduino/www/` | Old alias | **Legacy** |
| `/showduino/exports/` | Created unused | **Legacy**; canonical is `/showduino/export/` |
| `/showduino/devices/` | Fixture placeholders | **Keep** reserved |
| `/showduino/updates/` | Unused | **Legacy** |
| `/showduino/temp/` | Write probe | **Keep** |
| `/showduino/assets/` | — | **Add** |
| `/showduino/import/` | — | **Add** |
| `/showduino/recovery/` | — | **Add** |

Migration: if `e131.json` is missing, the nested `e131` object in `network.json` is
copied once (read-old / write-new). User files are never deleted automatically.

## Config rules

Every config file is versioned (`formatVersion: 1`), size-bounded, and rejected
as a whole if malformed. Missing file = firmware default. Never auto-resume a
show (`bootBehaviour` must be `idle`).

Writes use:

```text
validate path → preserve previous → write .tmp → flush → validate → replace
```

Previous copies land in `/showduino/recovery/<name>.prev` when possible.

## Health states

`ONLINE` · `DEGRADED` · `READ_ONLY` · `OFFLINE` · `FAULT`

Used on Serial, `/api/system`, `/api/storage`, and `HELLO` (`SD:<state>`).

## Failure behaviour

| Condition | Result |
|---|---|
| SD missing at boot | Boot continues. Comms UART, Director, emergency, minimum diagnostics work. State `OFFLINE` |
| Card removed at runtime | New writes stop. Loaded RAM timeline continues. Streamed audio may fault separately. Emergency continues |
| Corrupt config | File rejected. Firmware default. Boot continues |
| Write failure | Further writes stop until remount. Last error retained |
| Low space | Backup / log / diagnostics fail cleanly. Show RAM is unchanged |

Live cue, elapsed ms, E1.31 packets, heartbeats, and emergency debounce are
**never** written to SD.

## Logs

Buffered (~5 s or buffer full). Emergency lines flush immediately.
Rotate at 32 KiB; keep `name-001.log` … `name-003.log`.

Emergency log records activate, clear request, reject, expire, and success.
Those lines do not control the latch.

## Diagnostics and backup

`RUN:TEST` still writes `last-test.txt` and also `run-test-<session>.txt`.
`STORAGE:BACKUP` copies current config files to `/showduino/backups/config-<session>/`.
Production audio/assets are not copied.

## API and USB

- `GET /api/storage` and `storage` on `GET /api/system`
- USB: `STORAGE:STATUS` `STORAGE:LIST` `STORAGE:CHECK` `STORAGE:BACKUP`

No browser file explorer. No arbitrary path writes.

## Hardware tests still required

A. Normal SD boot  
B. No SD at boot  
C. Corrupt config  
D. Read-only / write-fail  
E. Load production, then remove SD  
F. RUN:TEST saved to diagnostics  
G. Config backup  
H. Power-cycle after config save  
I. Low-space  
J. WebUI storage status  
