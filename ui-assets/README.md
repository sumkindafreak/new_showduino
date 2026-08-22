# Showduino UI theme assets (Phase 2 DisplayManager)

## Runtime layout (SD card)

```
/showduino/ui/themes/<themeName>/theme.json
/showduino/ui/themes/<themeName>/<page>.bmp
```

Default theme used by firmware:

```
/showduino/ui/themes/default/theme.json
/showduino/ui/themes/default/desktop.bmp
```

## theme.json

```json
{
  "version": 1,
  "name": "Default",
  "author": "Showduino",
  "resolution": "800x480"
}
```

- `version` major must equal firmware `DISPLAY_THEME_MAJOR` (currently 1) or the theme is rejected.
- `resolution` must be exactly `800x480` (`DISPLAY_WIDTH`×`DISPLAY_HEIGHT`).
- Unknown fields are ignored (forward compatible).

## desktop.bmp (v1)

- Exactly **800×480**
- **24-bit** BI_RGB (uncompressed)
- BM signature; file large enough for padded rows + pixels
- Loaded into a single contiguous **PSRAM** buffer (no SRAM fallback)

## Design masters

Long-name PNG masters under `/showduino/ui/backgrounds/` (e.g. 1774×887) are **not** loaded at runtime.
Resize offline into the theme folder, e.g.:

```bat
ffmpeg -y -i "backgrounds\Futuristic show control dashboard screen.png" -vf scale=800:480 -pix_fmt bgr24 themes\default\desktop.bmp
```

## Desktop activation

If `theme.json` + `desktop.bmp` validate → Phase 2 DisplayManager Desktop.
Otherwise → legacy LVGL Desktop (asset gate).

## Dev stats

Define `SHOWDUINO_DISPLAY_STATS` (default on in DisplayTypes.h) for Serial counters:
`[Display]` / `[Theme]` / `[Background]` / `[Overlay]` prefixes.