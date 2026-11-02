# Showduino Studio Cue Library — creator guide

**Source-checked 30 October 2026.** This describes the **public Website Studio timeline Cue Library** in `sumkindafreak/showduino.com/js/timeline-fx-daw.js`, cross-checked with the Director source in `new_showduino`. The embedded S3 `/studio/` snapshot and installed firmware may differ. This is an authoring guide, not a claim of all-node hardware acceptance.

## Where the library lives

Open [Website Studio](https://show-duino.com/studio.html), open or create a production, and enter the **Timeline** editor. The **Cue Library** is in the timeline's track-list header. Use its arrow to expand/collapse the panel and its search field to filter presets.

**Do not confuse this with the Director Home tile labelled CUE LIBRARY.** In the checked `new_showduino/main` Director source, that tile is visible but **disabled**, with no active Cue Library page. The Director is for selecting/loading/running shows and viewing confirmed P4 status; it is not Studio's cue editor.

## Add a cue to a production

1. Add a compatible **Audio**, **Pixel**, **MOSFET**, or other track; a **Mixed Lane** can accept different cue types. A locked track cannot accept a dropped preset.
2. Expand **Cue Library**, search for a suitable preset (for example `Audio Cue`, `Ambient Loop`, `MOSFET PWM`, `Pixel Flicker`, `Pixel Lightning` or `Pixel Blackout`).
3. **Drag** the preset onto an unlocked matching-type track or Mixed Lane at the intended timeline position. With snapping enabled, drop time snaps to the timeline grid. Alternatively, **double-click** the preset to insert it at the current playhead onto the first eligible unlocked matching-type track (or Mixed Lane). If no suitable lane exists, Studio prompts you to add one.
4. Select the inserted clip and use the **Inspector** to check start/duration, parameters and routing. Assign the *actual* logical Node ID and output. For Pixel clips verify the selected line, start pixel and segment length; for Audio clips verify a real file exists on the appropriate node and that the path matches your installation. Preset filenames such as `impact.wav` and `ambience.wav` are examples, not an automatic asset upload.
5. Preview for creative timing, run **Project Check**, then save/export the `.shdo` production. A browser preview and a passed authoring check do **not** verify the physical P4 or nodes.
6. Use the Communications **S3 system console → Productions → Persist SHDO v2 to P4 SD** to attempt deployment, check the P4 result, separately **Load**, then use the Director to **START** only when the physical test area is ready. Supported cue types and compiler limits depend on the installed P4 firmware. See the [Quick Start](SHOWDUINO_QUICK_START.md#website-studio--installed-production).

## What the preset catalogue contains

The checked website code includes **Audio** (`Audio Cue`, `Impact Hit`, `Ambient Loop`), **MOSFET** (hold, pulse, PWM), **Pixel** (25 effect patterns plus exit-sign marker authoring preset), **FX** (including fog/air/vibration), **Trigger**, and **legacy Relay** authoring presets. Search matches names, types and Pixel effect families. Presets populate editable clip defaults; they do not certify hardware or availability.

**Important restrictions:** Not every Studio palette item is deployable by the current P4 production compiler. In particular, legacy Relay, Trigger and generic FX authoring choices should not be represented as supported active hardware cues without confirming the exact compiler and physical output. **DMX is parked**. The P4's separate **GPIO24 Emergency/signage line is safety-owned**, not a general-purpose show Pixel output; an exit-marker preset does not override Emergency policy. The P4 retains all show-timing and Emergency authority, and clearing Emergency never restarts a show automatically.

## Related reading

- [Quick Start — Website Studio to installed production](SHOWDUINO_QUICK_START.md#website-studio--installed-production)
- [User Manual — Studio and browser](SHOWDUINO_USER_MANUAL.md#10-showduino-studio-and-the-browser-interface)
- [Director Operator Guide](SHOWDUINO_DIRECTOR_OPERATOR_GUIDE.md)
- [30 October hardware/documentation evidence](SHOWDUINO_FINISH_LINE_STATUS_2026-10-30.md)

**Evidence:** `sumkindafreak/showduino.com/js/timeline-fx-daw.js` (Cue Library UI, search, presets, insertion/drag/drop); `js/studio-v4.js` (Project Check and deploy instructions); `sumkindafreak/new_showduino/firmware/director-esp32-8048s050/ShowduinoDirector8048S050/page_01_home.cpp` and `ShowduinoUi.h` (Director Home tile disabled). Website and firmware behaviour are source-checked, not end-to-end hardware-accepted.
