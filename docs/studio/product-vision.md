# Showduino Studio — Product Vision

**Phase:** 0 (Planning)
**Audience:** Product decisions for the authoring environment
**Companion OS vision:** `docs/showduino-product-vision.md`

---

## Identity

> **Showduino Studio is the production authoring environment for Showduino.**
>
> Creators build, validate, and deploy **Productions**.
> Operators run them from the **Director**.
> The **Stage Runtime** executes them safely.
>
> Studio never owns live show truth.
> Studio never executes cues.
> Studio never talks to pins.

That is the product's identity — not an architecture statement.

---

## Naming clarification

| Name | Meaning |
|------|---------|
| **Showduino Studio** | Authoring product (this blueprint) — create / edit / validate / deploy |
| **Director** | Runtime operator console (touchscreen desk) |
| **Stage Runtime / Show Engine** | Execution authority (ESP32-P4 Stage Controller) |
| **Web operator desk** (existing SoftAP UI) | Browser monitoring / command surface — **not** this authoring product |

The release capability name **Studio** in the OS roadmap (full ecosystem) remains a milestone label. This document defines the **authoring product** that that milestone eventually ships.

---

## Who it serves

| Persona | Need |
|---------|------|
| Show designer | Build cue lists, assets, lighting, audio without touching firmware |
| Venue technician | Assign logical devices, validate against installed fabric |
| Operator | Receives a validated Production on Director — not raw files |
| Integrator | Packages, deploys, rolls back, audits history |

---

## Decision filter

1. Does it make **creating or deploying** a production clearer, faster, or safer?
2. Does it fit the OS constitution (Ten Laws / ADRs), or invent a second truth?
3. Does it serve a **Production**, not a pin, board, or transport?

If any answer is no, it does not belong in Studio.

---

## Responsibility split (non-negotiable)

```text
┌─────────────────────┐
│  Showduino Studio   │  Author · Validate · Package · Deploy intent
└──────────┬──────────┘
           │ production package + deploy request
           ▼
┌─────────────────────┐
│     Director        │  Load · GO · Monitor · Emergency (requests)
└──────────┬──────────┘
           │ commands / confirmed state
           ▼
┌─────────────────────┐
│  Stage Runtime      │  Clock · Cue execution · Safety · SoT
└─────────────────────┘
```

Studio may **preview** and **simulate** behaviour for authoring confidence.
Simulation is not runtime truth (Law 1 / Law 6).

---

## What "Studio 1.0" means

Not every extension (MIDI, Art-Net, multi-user). **Authoring-complete** for a real production lifecycle:

| Area | 1.0 requirement |
|------|-----------------|
| Production Manager | Create, open, version, tag, readiness |
| Asset Manager | Import, categorise, preview, unused/broken refs |
| Cue List + Timeline | Edit timed cues with logical device targets |
| Triggers | Manual GO, auto, time, GPIO, conditional (authoring model) |
| Device Assignment | Logical devices / zones — never raw pins in the editor |
| Validation | Errors block deploy; warnings visible |
| Deployment | Package, transfer, verify, rollback |
| Runtime handoff | Director can load and run the deployed Production |

When every box is ticked, Studio 1.0 can do its primary job: take a show from blank project to live Director run.

---

## Success metrics

| Task | Target |
|------|--------|
| Create empty Production | ≤ 3 interactions |
| Import common asset | ≤ 5 seconds to searchable |
| Add cue on timeline | ≤ 2 interactions after intent |
| Validate production | One action; clear error list |
| Deploy to Stage path | Progress + pass/fail; no silent success |
| Operator finds show on Director | Same Production name / id as Studio |

---

## Relationship to OS 2.0

Studio is a **client of frozen platform contracts**, same family as Director:

- **Production Manifest v1** is the operator-facing object (ADR 0004).
- Intent reaches Stage via **Commands** (ADR 0002), never by Studio inventing runtime state.
- Events describe the past (ADR 0003); Studio may subscribe for deploy/runtime feedback but does not own ShowService truth.
- Storage backends evolve behind AssetService / Show Engine project store — Studio edits **Productions**, not "paths as product."

Director SD packages today are a **transitional** store. Studio's package format must map cleanly to that layout now and to Stage-owned project storage later without redesigning the authoring model.

---

## Out of scope for Studio

- Live cue clock ownership
- Emergency stop authority (Studio may send a request only if connected as a client; Director/Stage remain primary)
- Direct GPIO / DMX / audio engine control
- Replacing Communications Engine routing
- Silent "optimistic" success after deploy or GO

---

## Related documents

| Document | Role |
|----------|------|
| [Architecture](architecture.md) | How Studio is structured |
| [Production Format](production-format.md) | Package contract |
| `docs/showduino-product-vision.md` | OS product identity |
| `docs/constitution.md` | Hardware / SoT roles |
| `os2/Foundation.h` | Ten Laws |