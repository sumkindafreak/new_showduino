# showduino-release

## Purpose
Execute a safe release workflow without overstating validation.

## Workflow
1. Run preflight and capture start SHA/state.
2. Identify components changed (P4/Comms/Director/nodes/protocol/web/docs/tools).
3. Build affected targets with repository build commands.
4. Regenerate generated artifacts when source changes require it.
5. Run relevant host validation scripts/tests.
6. Update versions only where release policy requires.
7. Produce release summary with explicit compile/flash/bench status separation.

## Required release summary fields
- starting sha
- ending sha
- files changed
- targets affected
- compile status
- flash status
- bench test status
- acceptance test status
- compatibility impact notes

## Guardrails
- Do not auto-flash hardware.
- Do not claim bench verification without physical testing evidence.
