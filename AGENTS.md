# Showduino pixel defaults

- Every programmable pixel line starts with `SHOWDUINO_PIXEL_DEFAULT_COUNT` (10), unless a valid saved or explicitly commissioned count exists.
- Use `protocol/showduino_pixel_defaults.h` for the common default and validation. The standalone legacy AVR Mega sketch mirrors 10 in its local macro because its Arduino build cannot resolve external repository headers. Missing, zero, or invalid stored counts fall back to 10 within the device limit.
- Initialise the default line during boot. Do not leave an uncommissioned programmable line dark solely because its count was never saved.
- Preserve explicit physical counts for fixed status indicators and fitted lamp/signage assemblies; the Audio Node GPIO5 status indicator is one pixel.
- Emergency takes priority over effects, tests, ownership changes, and blackouts. Saved show brightness must not dim emergency white.
