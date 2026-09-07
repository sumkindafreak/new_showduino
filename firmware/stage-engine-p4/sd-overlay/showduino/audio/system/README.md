# P4 system / safety audio

Copy these WAV files onto the Stage Controller SD card:

```text
/showduino/audio/system/boot.wav
/showduino/audio/system/emergency.wav
/showduino/audio/system/beep.wav
/showduino/audio/system/tone.wav
/showduino/audio/system/error.wav
/showduino/audio/system/accepted.wav
/showduino/audio/system/complete.wav
/showduino/audio/system/shutdown.wav
```

Required format: PCM, 16-bit, mono or stereo, 44.1 kHz or 48 kHz preferred, 32 kHz accepted.

Bench source on this machine: `D:\showduino\audio\show_machine\`

The P4 plays a system sound only after the WAV header validates as PCM 16-bit mono/stereo at 32 / 44.1 / 48 kHz. It looks in `/showduino/audio/system/` first, then `/showduino/audio/show_machine/` for the same filename. Invalid or missing files are skipped; they do not brick boot.

Do not copy attraction/show files from `D:\showduino\audio\` onto the P4. Those belong on the Audio Node.

`boot.wav` plays when the Director touchscreen powers on (`HELLO` after the desk has been absent). It does not play on P4 power-up alone. USB `HELLO` does not trigger it.

`shutdown.wav` is reserved and unused. Missing optional notification files must not fail boot.

The P4 onboard ES8311 speaker provides Showduino system and safety audio only. Attraction/programme audio is exclusively produced by specialist Audio Nodes.
