# Showduino Studio — Runtime Integration Specification

**Phase:** 0 (Planning)
**Status:** Blueprint
**Constraint:** No speculative new wire protocols here — integrate with existing Stage/Director semantics

---

## 1. Purpose

Define how Studio relates to Director and Stage after (and around) deployment: load handoff, state observation, logs, and history — without Studio becoming a second Show Engine.

---

## 2. Communication principles

1. **Intent via Commands** — same family Director uses (`LoadProduction` / legacy `SHOW:LOAD` path, emergency requests, etc.).
2. **Truth via published state** — Stage `ShowRuntime` (and related publishes); Studio displays, does not invent.
3. **Transport is adapter** — ESP-NOW, UART, Wi-Fi SoftAP tunnel, future Ethernet — meaning unchanged.
4. **Acceptance ≠ completion** — deploy ACK ≠ cues firing; load ACK ≠ running.

---

## 3. Studio → Director

| Concern | Integration |
|---------|-------------|
| Catalogue | After deploy, Director Library sees Production via existing scan/manifest projection |
| Operator focus | Studio does not remote-drive Director UI chrome in v1 |
| Shared ids | Production `id` / `name` / `version` must match what Director shows |

Studio does not replace Director as the live desk.

---

## 4. Director → load → Stage (existing spine)

Documented current path Studio must remain compatible with:

```text
Operator selects Production on Director
  → LoadProduction / SHOW:LOAD:<id>
  → Director reads package (transitional SD) OR Stage serves package (target)
  → SHOW:TL:BEGIN / SHOW:TL:C:… / SHOW:TL:END (when Director uploads)
  → Stage owns SHOW_LOADED / RUNNING / …
  → SHOW:RUNTIME mirror to Director
```

Studio's job is to ensure the package Stage/Director consume is valid and present — not to own this clock.

---

## 5. Runtime state returned to Studio

When Studio is connected for post-deploy verify or tech rehearsal observe:

| Source | Studio may show |
|--------|-----------------|
| ShowRuntime fields | state, cue index, elapsed, flags, revision, errors |
| Link / node health | as published by fabric services |
| Deploy status | from Deployment module |

Studio must label mirrored data as **live mirror**, not authoring draft.

---

## 6. Logs

| Log class | Owner | Studio |
|-----------|-------|--------|
| Authoring / validate / deploy | Studio | Primary |
| Operator actions | Director | Optional sync pull |
| Execution / faults | Stage | Optional sync pull |

Synchronisation is **append pull / export**, not dual-write SoT. Conflict policy: Stage/Director logs win for runtime; Studio logs win for authoring.

---

## 7. Deployment history

See Deployment Specification. Runtime integration only requires:

- Studio can query last successful deploy for a Production id
- Director can show version string from Manifest
- Mismatch (Director loaded version ≠ last Studio deploy) surfaces as Warning in Studio observe mode

---

## 8. What Studio must never do

- Run the cue clock locally while claiming Stage authority
- Mark relay/audio success from a sent request alone
- Bypass validation to force deploy
- Embed MAC addresses as cue identity (logical device ids only)

---

## Related

- [Deployment](deployment.md)
- `protocol/showduino_show_runtime.h`
- `docs/command-protocol.md`
- `docs/state-synchronisation.md`
- ADR 0002 / 0003