# showduino-bench-test

## Purpose
Generate a human bench-test sequence from changed components without falsely claiming physical execution.

## Workflow
1. Identify changed subsystem(s) and affected interfaces.
2. Build a stepwise bench sequence: setup, action, expected observations, failure checks.
3. Include emergency/safety regression checks when relevant.
4. Include offline/degraded path checks when networking/control paths are touched.
5. Mark each step by test type (BENCH TEST, MANUAL UI TEST, HOST TEST, AUTOMATED, NOT YET IMPLEMENTED).
6. Explicitly state that physical testing requires human/hardware execution.

## Guardrails
- Never report bench tests as passed unless physically run.
- Separate compile results from hardware behavior validation.
