# P4 ambience emergency tests

Run `bash tools/ambience-tests/run_tests.sh` with a C++17 compiler. The tests compile the actual `StageAmbience.cpp` against an in-memory SD filesystem and simulated I2S driver.

Coverage: normal-to-emergency WAV replacement, canonical/fallback assets, invalid/missing WAV, absent SD, I2S allocation failure, repeated assertion, command rejection while latched, emergency software volume despite saved mute, authorised clear without restore, and fresh playback after clear.

These tests do not establish DAC electrical behaviour, radio delivery, firmware-target compilation, stereo/mono fidelity, DMA continuity or simultaneous ES8311 playback. Use a real stereo PCM emergency WAV and verify all audio paths on the bench.
