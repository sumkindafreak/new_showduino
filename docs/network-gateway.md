# Comms home / venue Wi-Fi gateway

Product version: Showduino **1.0.0-rc.1**

The Communications S3 owns external connectivity. The P4 is not the authority
for home Wi-Fi, internet, or GitHub. Losing internet is **not**
`SHOWDUINO CONNECTION LOST`.

## What stays up

- SoftAP SSID `Showduino` remains available in AP-only and AP+STA.
- ESP-NOW follows the operating channel. Peers are never deleted because STA
  associated or a node joined.
- ESP-NOW is never deinitialised to “fix” Wi-Fi.

## Channel authority

| STA state | Operating channel |
|-----------|-------------------|
| Associated to home/venue AP | Venue AP channel (SoftAP + ESP-NOW follow) |
| Idle after ~3 s settle | Home channel 1 |

Director, Audio Node, and Lamp Node follow SSID `Showduino` by Wi-Fi scan
**only while unlinked**. They do not pick a conflicting channel independently.

## Operator controls

WebUI Network page and HTTP:

| Method | Path | Effect |
|--------|------|--------|
| GET | `/api/gateway` | AP / STA / internet / channel (no passwords) |
| POST | `/api/gateway/scan` | Async scan |
| POST | `/api/gateway/connect` | Save SSID/password in NVS `sdnet`, start STA |
| POST | `/api/gateway/disconnect` | AP only, keep credentials |
| POST | `/api/gateway/forget` | Wipe credentials |
| POST | `/api/gateway/mode` | `ap_only` or `ap_sta` |

Passwords are never logged or returned.

## GitHub Releases check

`GET /api/updates` and `POST /api/updates/check` discover tags from
`sumkindafreak/new_showduino` `/releases?per_page=8`, skipping drafts.

- No OTA install.
- `/releases/latest` is not used (it ignores prereleases).
- No published releases → status `no_releases`, not a Showduino fault.
- Unauthenticated GitHub API is rate-limited.

Director shows `STATE:GATEWAY:` and `STATE:UPDATE:` as **LINK vs WIFI vs UPD**.
Update availability is informational. No blocking prompt. No LED flash.

Home Wi-Fi is optional. A loaded production must run without STA or internet.
