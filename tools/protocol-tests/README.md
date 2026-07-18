# Protocol host tests

Compiles shared `protocol/` headers on the host (no Arduino, no ESP32 SDK).

## Build and run

From this directory:

```bash
g++ -std=c++17 -Wall -Wextra -I../../protocol -o protocol_tests test_protocol.cpp
./protocol_tests
```

Windows (PowerShell, with MinGW or similar on `PATH`):

```powershell
g++ -std=c++17 -Wall -Wextra -I../../protocol -o protocol_tests.exe test_protocol.cpp
.\protocol_tests.exe
```

Or use `run_tests.ps1` / `run_tests.sh` in this folder.
