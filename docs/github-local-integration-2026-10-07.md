# GitHub and local firmware integration — 7 October 2026

This build combines the morning PR #26 (`cfa1512`), the earlier Audio / Outputs / Emergency / Director delivery changes through PR #25 (`12d304c`), and the latest fetched main (`f596d65`). The latest main Studio authoring snapshot is retained; the commissioning bundle is regenerated from the combined source so its new ambience and GPIO controls are present.

The P4 production loader accepts Emergency Node pixel cues, with one shared check restricting the ESTOP cue type to `ESTOP:NODE:PIXEL:`. Show files cannot assert, clear or rearm emergency state through that cue type. Both branches had independently added this check; the duplicate expressions were removed during integration.

The new ambience and GPIO policy headers use the same sketch-relative include convention as the existing protocol headers. This fixes an Arduino build failure caused by paths that only resolved from the original source tree. The ambience host test includes the sketch directory as well, matching the firmware build's header search path.

The local Director Nodes page update coalescing is retained. Incoming state and heartbeats are serviced before display work, and the Nodes page redraws at most once per 100 ms when its data changes. The new P4 ambience status handler remains connected to the Audio System page.

| Firmware | Integrated version |
| --- | --- |
| Director | 0.9.13-director |
| P4 Stage Engine | 0.6.12 |
| S3 Comms Controller | 0.5.7 |
| Audio Node | 0.4.7 |
| C3 Emergency Node | 0.3.2 |

Comms 0.5.6 belongs to the separate, paused OTA foundation work. That work remains on its existing feature branch and in the local backup; this integration does not enable system-wide OTA or publish firmware releases. The distinct 0.5.7 version avoids assigning the same version to different binaries.

To bench-test the combined behavior, rebuild and flash Director, P4 and Comms together. Audio Node 0.4.7 and Emergency Node 0.3.2 already include their earlier fixes; update those only if the hardware is running an older version. The Audio Node's saved LINE output selection remains the known working jack configuration. Flashing and hardware testing are separate from synchronizing the source checkout.

The original local working tree was archived before switching branches. The archive includes all 93 modified/untracked files, including paused OTA changes, original reference-name edits, the bench package and the enclosure model files. Saved pin values and touch calibration defaults were compared before the switch; the remaining reference-name edits were comments and log wording.

The regression checks cover Director upload delivery and run authorization, MOSFET feedback, P4 GPIO and bus assignments, Emergency discovery, headphone transitions, pixel commissioning, Studio pixel authoring and browser Outputs controls. The GitHub host workflow also exercises actual C++ ambience playback and SHDO-to-production parsing.
