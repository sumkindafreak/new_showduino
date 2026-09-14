# Comms Controller self-OTA (Phase 2A)

```text
Status: SOFTWARE implemented / physical proof is a separate gate
Product: Showduino 1.0.0-rc.1
Protocol: 1.0
SHDO: v2 unchanged
Comms: 0.5.1
Hardware ID: SHOWDUINO-S3-COMMS-V1
```

This is **Comms self-OTA only**. System-wide OTA does not exist. P4, Director, Lamp, Audio, Pixel, Emergency, MOSFET, and Relay still require USB.

## Why Comms first

The dedicated ESP32-S3 Comms Controller already hosts GitHub discovery, the Update Manager, and the WebUI. It is the cleanest first OTA target.

## Partition audit (do not change)

Production FQBN:

```text
esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=8M,PSRAM=opi,PartitionScheme=default_8MB
```

`default_8MB` already provides dual OTA slots:

| Name | Offset | Size |
|------|--------|------|
| nvs | 0x9000 | 20 KB |
| otadata | 0xe000 | 8 KB |
| app0 / ota_0 | 0x10000 | 3,342,336 |
| app1 / ota_1 | 0x340000 | 3,342,336 |
| spiffs | 0x670000 | 1.5 MB |
| coredump | 0x7F0000 | 64 KB |

Studio / WebUI is compiled into firmware (`WebAssets.generated.h` / PROGMEM). Do not shrink SPIFFS or NVS for OTA.

Arduino core 3.3.11. Bootloader auto-rollback (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`) is **not** enabled in this core package. Rollback uses `Update.rollBack()` plus reboot after the health gate fails. `esp_ota_mark_app_valid_cancel_rollback()` runs only after the health gate passes.

## Native OTA guarantees

- The active slot remains bootable until `Update.end(true)` selects the candidate.
- Power loss during download or a partial inactive-slot write must not switch boot.
- Power loss after `Update.end(true)` but before reboot: next boot is the candidate, which enters PENDING VALIDATION.
- Power loss during validation: candidate is still pending; timeout or next boot retries the health gate, then rolls back if it fails.
- SHA-256 is computed over the streamed bytes. Mismatch aborts **before** boot switch (`FIRMWARE_INTEGRITY_FAILED`).

HTTPS uses the existing Comms stack (`WiFiClientSecure` + `setInsecure()`). SHA-256 proves the binary matches the supplied manifest. It does **not** prove publisher authenticity. Signed manifests are not implemented.

## Lifecycle

```text
KNOWN GOOD
    |
download HTTPS candidate (streamed, 1 KB chunks, cooperative delay)
    |
SHA-256 == manifest
    |
write inactive OTA partition
    |
persist transaction (from/to/pending)
    |
STATE:UPDATE:COMMS:REBOOT_REQUIRED
    |
reboot
    |
PENDING VALIDATION (20 s)
    |
health gate
    |
PASS -> mark valid -> COMPLETE
FAIL -> Update.rollBack() -> previous firmware -> ROLLED_BACK
```

Validation timeout: **20000 ms** (`SHOWDUINO_COMMS_OTA_VALIDATE_MS`). P4 link timeout is 8 s. Internet is **not** required for health.

Health gate:

* booted
* UART initialised
* fresh P4 link
* ESP-NOW initialised
* SoftAP / network stack up
* WebUI alive
* no fatal local fault

Specialist nodes are not required.

## Preconditions

Production apply requires:

* `component=comms`
* hardware ID `SHOWDUINO-S3-COMMS-V1`
* installed < candidate (same version needs compile-time `SHOWDUINO_OTA_ALLOW_FORCE`, not exposed in the operator UI)
* no automatic downgrade
* operator `confirm=true`
* P4 alive
* show not running
* no global emergency
* P4 SYSTEM MAINTENANCE observed (`UPDATE:MAINTENANCE:ON`)

Generic `POST /api/updates/apply` without `component` still returns 501. P4 / Director / nodes return `OTA_UNAVAILABLE`.

## Emergency

* P4 GPIO25 hardwired NC remains independent of Comms.
* Emergency during CHECK / DOWNLOAD / VERIFY / WRITE: abort, do not boot a partial candidate, state `INTERRUPTED_BY_EMERGENCY`.
* OTA writes are chunked with `vTaskDelay` so UART / ESP-NOW keep running on the main loop.
* Wireless nodes may **ASSERT** emergency. They must never **CLEAR**.
* Emergency Node OTA remains disabled.

## Comms reboot window

During the Comms reboot, wireless specialist nodes temporarily lose the transport bridge. This is expected.

* P4 remains running.
* Hardwired P4 GPIO25 remains functional.
* Director may lose the Comms link and should show COMMS RESTARTING / RECONNECTING from last-known state.
* This is not an unexplained system failure.

## Bench / GitHub candidate

Normal delivery is a GitHub Release, not a local HTTPS server.

1. Bump `SHOWDUINO_COMMS_FIRMWARE_VERSION` in `BoardConfig.h`.
2. Build with `tools/release/make_comms_artifact.ps1` (reads the version, writes size/SHA-256 into the manifest and `releases/artifacts/comms-artifact.json`).
3. Publish the generated `.bin` and `showduino-1.0.0-rc.1.manifest.json` with `tools/release/publish_comms_github_release.ps1`.
4. On the bench: Network → connect venue Wi-Fi (AP+STA). SoftAP and ESP-NOW stay up.
5. Diagnostics → SHOWDUINO SOFTWARE → Check for Updates.
6. Confirm Communications Controller available > installed, then INSTALL UPDATE / Update Comms.

Check for Updates reads GitHub Releases, downloads the Showduino manifest, maps `ShowduinoS3CommsController.ino.bin`, and fills firmware / size / SHA-256. Apply still requires operator confirmation, P4 maintenance, and the existing safety gates.

A board that is still running Comms 0.5.0 cannot yet auto-fill those fields (discovery of the comms component landed in 0.5.1). Use the GitHub `browser_download_url` plus the printed SIZE / SHA256 on that board's existing Update Comms form. Do not USB-flash Comms for the 0.5.0 → 0.5.1 proof.

The apply API rejects non-`https://` URLs.

## Firmware version reuse

Keep `SHOWDUINO_COMMS_FIRMWARE_VERSION` unique per accepted binary.

* A prerelease candidate that has **never** been physically installed may be replaced under the same version (this 0.5.1 Network-page fix is that case).
* Once a firmware version has been physically installed, or otherwise distributed as an accepted build, **never** replace it with a different binary under the same version string.
* The next post-install firmware change must be `0.5.2` or later.

## Network page (venue Wi-Fi)

The Network page keeps SSID/password in transient page draft state so status polling cannot wipe the form. Connect captures credentials **before** any busy/repaint. Scan selection writes the draft, not a throwaway DOM node. Passwords stay in the password input / draft only: never in status JSON, logs, or `localStorage`.

Manual check after a corrected 0.5.1 OTA:

1. Join Showduino AP and open Network.
2. Type an SSID and password; wait through several status refreshes. Text must remain.
3. Scan, tap a network, wait through a refresh. SSID must remain; password field should take focus.
4. Connect. SoftAP stays up. Failure must show the API error. Success may clear the typed password; SSID remains.

Live 0.5.0 still has the old form bug. Do not expect that board's WebUI to keep typed credentials.

## Intentional rollback test

Compile a **throwaway** candidate with:

```text
-DSHOWDUINO_OTA_TEST_FAIL_HEALTH=1
```

That image boots, fails the health gate, never marks valid, and rolls back. Do **not** leave this flag enabled on `main` or in production builds.

## USB recovery

OTA must never remove USB recovery.

1. Hold **BOOT**, tap **RESET**, release **BOOT** (or use the board USB-CDC download mode).
2. Flash known-good firmware with the production FQBN and `PartitionScheme=default_8MB`.
3. Keep both OTA slots. Do not flash a single-app scheme over an OTA-capable board unless you intend to USB-migrate again.

```text
arduino-cli upload --fqbn "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=8M,PSRAM=opi,PartitionScheme=default_8MB" --port COMx firmware/s3-comms-controller/ShowduinoS3CommsController
```

Destructive recovery is not exposed in the operator UI.
