# AGENTS.md

## Cursor Cloud specific instructions

This repo is mostly ESP32/Arduino/ESP-IDF **firmware** (`firmware/`, `stage-engine/`)
which targets microcontrollers and cannot be built or run inside the Cloud VM
(no MCU cross-toolchains, no hardware). The parts that are runnable/testable on the
host are the web apps under `web/` and the C++ protocol tests under `tools/`.

Standard commands live in each project's `package.json` / README; only non-obvious
caveats are noted here.

### `web/gorefx-dashboard` — GoreFX Studio (Vite + React + TS)
- Primary host-side app. Dev server: `npm run dev` (Vite, serves on `0.0.0.0:5173`).
  Dev mode uses esbuild and does **not** type-check.
- `npm run build` runs `tsc && vite build`. Two non-obvious gotchas, both handled by
  the update script (install-time, `--no-save`, no repo edits):
  - `package.json` pins deps to `"latest"`, so a plain `npm install` pulls
    TypeScript 7.x, which rejects this repo's `tsconfig.json`
    (`moduleResolution: "Node"`). The build needs `typescript@5`.
  - `package.json` omits `@types/react` / `@types/react-dom`, so `tsc` fails with
    JSX `IntrinsicElements` errors until they are installed.
- The app runs standalone with a mock command bus (no backend needed). Clicking Live
  Control buttons appends to the in-app Command Log.

### `tools/protocol-tests` — C++ host tests
- `bash tools/protocol-tests/run_tests.sh` compiles the shared `protocol/` headers
  with system `g++` (C++17) and runs the assertions. No dependencies to install.

### `web/showduino-studio` — Studio WebUI (static ESM)
- Vanilla ES-module app normally served by the ESP32-C3 with a live `/api` + WebSocket
  backend. No build step / no package.json. For a local preview serve the folder as
  static files (e.g. `python3 -m http.server 8080` from `web/showduino-studio`).
  Without the firmware backend the app shell/nav renders but data pages report
  `/api/...` fetch errors — expected off-device.

See `docs/development-workflow.md` and `docs/architecture.md` for the firmware
architecture and device roles.
