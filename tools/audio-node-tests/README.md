# Audio Node host tests

Parser, path, volume, state, emergency, and timeout rules from `protocol/showduino_audio_node.h`. No hardware required.

The runner also checks the 0.4.8 amplitude-to-DAC volume curve and compiles the
actual codec volume/mute functions and playback service against mocked I/O.
Coverage includes eight-second Emergency repeat gaps for all supported WAV
rates, mono/stereo, consecutive repeats, short writes, stop cancellation and
unchanged immediate normal loops. These checks cannot establish audible output
on physical hardware.

```text
powershell -File tools/audio-node-tests/run_tests.ps1
```
