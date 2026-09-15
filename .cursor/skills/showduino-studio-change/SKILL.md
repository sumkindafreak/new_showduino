# showduino-studio-change

## Purpose
Safely change Studio/WebUI while preserving architecture, mobile usability, and embedded build consistency.

## Workflow
1. Inspect existing page/component behavior before editing.
2. Identify mobile and desktop UX impact.
3. Identify source files (`web/showduino-studio/`) versus generated assets.
4. Edit source files only; avoid hand-editing generated headers.
5. Regenerate embedded assets via `python tools/embed-webui/embed_webui.py` when needed.
6. Validate WebUI/API boundary behavior (UI requests vs P4 authoritative state).
7. Validate polling/repaint does not destroy active form input.
8. Validate credentials/passwords are not leaked to status JSON/logs/browser persistence.
9. Report whether embedded firmware asset output changed.

## Guardrails
- Mobile is first-class.
- UI state is not authority.
- Running-show continuity must remain independent from browser connectivity.
