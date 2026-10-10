# Showduino finish-line manual status — 10 October 2026

The existing commercial manual revision 0.4-RC describes the 7 October baseline. The newer, unmerged PR #29 combines PR #27 and PR #28, with P4 0.6.12, Comms 0.5.7 and Director 0.9.13-director source versions. These are not confirmed installed firmware versions.

PR #29 host and embedded Studio checks passed, but physical LOAD → RUN → COMPLETE acceptance has not been recorded. The fixed 22-second scenario is in `examples/productions/all-nodes-bench-test.shdo`; follow `examples/productions/ALL_NODES_BENCH_TEST.md`.

Ten pixels are the default for P4, C3 Pixel Node, Audio Node show pixels and Emergency Node show pixels, unless valid saved settings override them. Lamp Jewel remains seven pixels.

Still to verify physically: complete scenario playback, Audio Node continuity, Director ambience, Emergency Node white pixels, Lamp emergency sound, MOSFET output and Director Nodes-page stability. Re-run the scenario with Emergency asserted and confirm no automatic resume.

For the latest integration/CI/merge status, see `docs/pr27-pr28-integration-2026-10-10.md`. Do not treat the current `main` branch alone as the completed integrated firmware.
