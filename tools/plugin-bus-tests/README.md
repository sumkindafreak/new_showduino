# Plug-in Bus role host tests

Host tests for I²C identity versus configured role. They do not use the Arduino
or ESP32 SDK.

The suite covers display names, chip/role compatibility, `plugin-bus.json`
validation, and acceptance cases A–G (built-in ES8311, configured SX1509
inputs/outputs, unconfigured SX1509, configured-but-absent SX1509, illegal
SX1509 audio role, and unknown addresses).

Run on Windows:

```powershell
.\run_tests.ps1
```

Run on Linux/macOS:

```bash
./run_tests.sh
```
