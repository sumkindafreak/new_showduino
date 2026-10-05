# Audio Node bench fixes (firmware 0.4.4)

## Director layout

Page 05 anchors the playback and utility rows above `OS_DOCK_Y`: playback 294-342, utility 350-394, dock starts 402. Status is 98-198 and microphone summary 206-286. Microphone controls live in their own popup, retaining all five controls. Asset picker footer positions derive from its own height; no action is below the dock.

## Director touch

GT911 release is filtered for 60 ms. Page changes and wake-up consume the current touch and require 180 ms continuously released before another tap reaches the UI. LVGL also waits for release when the page changes. Bench check: tap Nodes once, hold briefly, then release; it must stay on Nodes, and a deliberate second tap must open Pixel Node. Scrolling and press/hold controls must still work.

## SD inventory

Opening SELECT ASSET requests inventory page zero. P4 requests the same inventory mechanism periodically instead of requesting a LIBRARY reply it does not consume. Inventory recursively includes WAV files under `/showduino/audio`, including `show_machine`. One complete relative path (up to 63 characters) travels per page so neither the 96-byte radio message nor the Director mirror truncates it. NEXT PAGE uses the same page size. `/The_Chamber` show-library commands remain separate; this change does not redirect playback paths to that root.

## Pixel outputs

GPIO5 is the fitted one-pixel status indicator; GPIO22 is the programmable strip. Each has its own checked RMT channel and explicit init/write failure logs. The strip defaults to ten pixels at boot; valid saved counts take precedence. Emergency bypasses saved brightness, and blackout cannot erase an active emergency frame. Pixel status uses `PIXEL:STATUS` so P4 does not parse it as audio playback status.

## Bench acceptance after upload

1. Upload Audio Node 0.4.4, updated P4, and updated Director. Check boot reports GPIO5 output ready and GPIO22 ready with count ten or the saved value.
2. Confirm both Director action rows remain above the dock and all microphone controls work in their popup.
3. Open SELECT ASSET, confirm nested WAV files appear, and page through the inventory without skipping names. Select and play a known asset.
4. Test GPIO5 independently. With the strip idle and emergency cleared, set its actual count in the Audio WebUI, initialise, and test it through P4/show control or in standalone mode.
5. Activate emergency from P4: both outputs must be white while emergency audio plays. Clear must stop the override without resuming prior effects. Reboot must preserve a valid saved strip count.

Compilation and software checks do not replace physical verification of the connected LEDs, SD card and display.
