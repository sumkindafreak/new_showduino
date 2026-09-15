# showduino-preflight

## Purpose
Run before substantial Showduino work to capture repository state, scope, risk, and validation path.

## Workflow
1. Report current directory and absolute repository path.
2. Report current branch and HEAD SHA.
3. Inspect git status and list dirty/untracked files.
4. Identify affected subsystem(s) and ownership boundaries.
5. Identify current version source(s) for affected component(s).
6. Identify exact build command(s) for affected target(s).
7. Identify affected interfaces (protocol/API/UART/ESP-NOW/storage/UI).
8. Identify relevant host tests and bench/manual tests.
9. Explicitly confirm unrelated dirty work will be preserved.

## Output checklist
- repository path
- branch
- sha
- dirty files
- subsystem scope
- version source
- build commands
- affected interfaces
- test plan
- dirty-work preservation statement
