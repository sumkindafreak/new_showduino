# Dedicated system sound volume

Settings > Audio > Local output has a dedicated 0–100% system volume slider and Refresh volume button. Release the slider to save; Director shows the level reported by P4 rather than assuming delivery. If a response is lost, Refresh volume queries P4 again. A newer Director with an older P4 leaves the slider unavailable until it receives the new status response.

P4 stores the setting in NVS (`system-audio` / `volume`), restores it before boot sounds, and defaults to the existing 80%. It applies to boot, beep, tone, error, accepted and complete sounds on the onboard speaker. Zero mutes these sounds. Ambience and specialist Audio Node levels are independent. Real emergency audio and emergency audio tests always use the existing fixed 80% level. Clearance and show playback are unaffected.

Commands: `AUDIO:SYSTEM:VOLUME?` queries, `AUDIO:SYSTEM:VOLUME:<0-100>` saves, and `AUDIO:SYSTEM:VOLUME:<level>` reports the saved level. Invalid arguments and storage/output failures return explicit errors. Both updated P4 and Director firmware are required.

Validation: host tests exercise every valid volume, invalid/malformed inputs, and fixed emergency output at all system levels. Director emergency-clearance regression tests remain in the runner. Full firmware builds, NVS persistence across actual power cycles, slider layout/touch interaction, codec behavior at mute/full level, emergency preemption and independent ambience/show playback require hardware verification. No firmware was flashed.
