# Showduino status-colour standard

This document defines the semantic colour language for **status / identifier indicators** in current Showduino firmware. It does not restrict theatrical show colours.

## Priority

Highest state wins:

1. **Emergency** — solid full-brightness white.
2. **Fault** — magenta ↔ amber.
3. **Identify / Locate** — bright moving or flashing white. Solid white is reserved for Emergency.
4. **Recording** — magenta pulse where implemented.
5. **Playing / active output** — green.
6. **Paused** — amber.
7. **Connecting / searching** — amber pulse or movement.
8. **Owned** — dim turquoise.
9. **Linked** — dim cyan and only while the communications link is fresh.
10. **Ready** — dim violet.
11. **Boot** — violet pulse / boot presentation.

Green is not a generic “healthy” colour. It means a real active output or running/active presentation.

## Current indicators

| Component | Indicator | Harmonised behaviour |
| --- | --- | --- |
| Comms S3 | onboard RGB, configured GPIO48 | Boot violet pulse; connecting amber; synchronising cyan pulse; healthy/linked dim cyan; faults magenta↔amber; Emergency solid white. |
| Director | 2 ambient pixels, GPIO17 | Boot/ready violet; connecting amber; running green; paused/warning amber; Fault magenta↔amber; Locate flashing white; Emergency solid white. UI branding remains independent. |
| MOSFET Node | 4 identifier pixels, GPIO25 | Active output green; Identify white chase; linked cyan only while fresh; owned turquoise; fault magenta↔amber; Emergency solid full-white while MOSFET power outputs remain OFF. |
| P4 | GPIO24 emergency/signage | Normal signage behaviour is unchanged; Emergency is full-white at 255. |
| P4 | GPIO23 Show Pixel Line | Theatrical colours unrestricted; Emergency bypasses saved show brightness and forces 255 white. |
| C3 Pixel Node | GPIO2 shared show line + OLED | Theatrical colours unrestricted; Locate is moving white; Emergency is 255 white; OLED link state uses fresh traffic. |
| C3 Emergency Node | GPIO2 shared line + OLED | System or local Emergency receives the same white override; remote/system Emergency has a dedicated OLED screen; link state uses fresh traffic. |
| Lamp Node | 7-pixel Jewel, GPIO8 | Functional lamp element, not a general status indicator. Existing boot RGBW test, warm-white Identify walk and 255-white Emergency are retained. |
| Audio Node | GPIO22 programmable pixel line | Theatrical colours unrestricted; Locate is moving white; Emergency bypasses saved brightness and forces 255 white. |

## Link freshness

“Linked” means recent validated Showduino traffic, not “has connected at least once since boot”. C3 Pixel, C3 Emergency and MOSFET status presentation use rollover-safe elapsed-time freshness against the existing radio freshness timeout.

Loss of link does **not** clear Emergency and does not invent a new Emergency state.

## Emergency scope

This harmonisation changes indication, not Emergency authority. It does not redesign physical button handling, assertion, latching, authorised clearing, output shutdown or Director Locate timing.

MOSFET power outputs remain fail-safe OFF in Emergency even though their **identifier pixels** are white.

## Audio hardware note

Audio Node has a separate **one-pixel status indicator on GPIO5**, a programmable show strip on **GPIO22**, and amplifier enable on **GPIO21**. KEY6 / Next is disabled because it shares GPIO5. Status follows the priority and colour standard above; `STATUS:LED:TEST` runs a non-blocking RGB test, while `PIXEL:TEST` tests the show strip. Status works independently of the saved show-strip count. Emergency overrides status testing with full-white. Hardware acceptance must verify GPIO5 is isolated from the KEY6 switch circuit before pressing that switch.

## Bench acceptance

Before merge/flash acceptance, compile every modified target and physically verify:

- Emergency is solid white and cannot be dimmed by saved pixel brightness.
- Identify/Locate is visibly moving/flashing white and never confused with solid Emergency.
- Fault is visually distinct from Emergency.
- Green appears only for real active/running output semantics.
- Linked indication expires after the existing freshness timeout and returns after valid traffic.
- MOSFET outputs remain OFF during Emergency while identifier pixels are white.
- Director Locate acknowledgement and Emergency clear remain separate behaviours.
- Audio playback, ESP-NOW, UART, touch, WebUI and theatrical pixel effects remain functional.
