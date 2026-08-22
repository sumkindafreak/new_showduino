# The Showduino Product Vision

> **Showduino OS is the operating system for live entertainment control.**
>
> It provides a consistent, hardware-independent operator experience for
> productions of every size—from school shows and scare attractions to
> theatres, museums and permanent installations.
>
> Operators interact with productions, not hardware.
> Applications present truth, services own truth, commands express intent,
> and the Stage Runtime executes safely.
>
> Every feature must make operating a live production clearer, faster or safer.

That is the product's identity—not an architecture statement.

When you wonder **"should we build this?"**, start here.

---

## The decision filter

1. Does it make operating a live production **clearer, faster, or safer**?
2. Does it **fit the platform** (Constitution / Ten Laws), or bend it?
3. Does it serve a **Production**, not a piece of hardware?

If any answer is no, it does not belong in Showduino OS.

---

## What "1.0" means

Not feature-complete. **Operator-complete.**

Showduino OS 1.0 can run a real production from start to finish.

| Area | 1.0 Requirement |
|------|-----------------|
| Dashboard | Live and stable |
| Library | Load productions reliably |
| Lighting | Operate fixtures and test outputs |
| Audio | Playback and control |
| Network | Device health and diagnostics |
| Safety | E-stop and recovery workflow |
| Settings | Persistent configuration |
| Session | Restore previous state |
| Stage Runtime | Reliable execution |
| Production | Validated before launch |

When every box is ticked, that is 1.0—because the system can do its primary job, not because every idea is built.

---

## Operator metrics (Law 10)

| Task | Target |
|------|--------|
| Load a production | <= 3 interactions |
| Start a show | <= 2 interactions |
| Emergency stop | 1 interaction |
| Find a production | <= 5 seconds |
| Identify a node fault | <= 10 seconds |

---

## Engineering quality gates

| Gate | Target |
|------|--------|
| Crash-free sessions | 100% |
| Command acknowledgement | < 100 ms locally |
| Startup to ready | under 5 seconds |
| Event delivery | ordered and loss-free |
| State recovery after reconnect | automatic |
| No duplicate truth | enforced by code review |

These are gates, not aspirations.

---

## Release philosophy

Do not number releases by code milestones alone. Name them by **capability**.

| Name | Meaning |
|------|---------|
| **Foundation** | Architecture Frozen / OS 2.0 Complete |
| **Operator** | Complete operator workflow (power-on to shutdown) |
| **Production** | Production ecosystem |
| **Network** | Multi-client support |
| **Studio** | Full ecosystem |

Semantic versions (`2.1`, `2.2`, …) still apply. Internally, the team shares what each release is trying to achieve.

Current: **Foundation** (`2.0.0` — OS 2.0 Complete).

---

## Related documents

| Document | Audience |
|----------|----------|
| [Product Vision](showduino-product-vision.md) (this file) | Product decisions |
| [Roadmap](showduino-os-roadmap.md) | Phases A–E |
| [Constitution](../firmware/director-esp32-8048s050/ShowduinoDirector8048S050/os2/Foundation.h) | Contributors before coding |
| [ADRs](adr/) | Why decisions were made |
| [Compatibility](../firmware/director-esp32-8048s050/ShowduinoDirector8048S050/os2/Compatibility.h) | Public API versions |
| [Showduino Studio Phase 0](studio/README.md) | Authoring product blueprint (create / validate / deploy) |

---

## Changelog — this milestone

> **Showduino OS 2.0 Complete** — The platform is established. Future development focuses on delivering operator capabilities within a stable architectural constitution.
