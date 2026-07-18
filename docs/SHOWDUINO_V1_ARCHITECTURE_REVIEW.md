# Showduino v1 — Brutal Architecture Review

Showduino has reached the point where it is no longer just a cool electronics project. It is becoming a real show-control product.

That is exactly the point where it is normal to question every software decision made along the way.

## What appears to be right

- The ESP32-P4 acting as the authoritative stage engine.
- The Director touchscreen being replaceable rather than being the single brain of the system.
- An offline-first approach suitable for live attractions and installations.
- Emergency stop behaviour taking absolute priority.
- Commands using acknowledgement rather than a fire-and-forget model.
- Clear separation between control, display, transport and node responsibilities.

## The real source of the concern

Showduino is no longer just making lights flash.

It now includes:

- scene control
- timelines
- audio
- DMX
- GPIO
- networking
- discovery
- file formats
- touchscreen UI
- safety systems
- multiple processors
- multiple node types
- fallback behaviour

That level of complexity makes the project feel risky even when the core design is sound.

The danger now is not necessarily that the architecture is wrong. The bigger danger is rewriting everything because the project suddenly feels large.

## The key v1 question

> If somebody bought Showduino tomorrow, could they create and run a show without reading the source code?

If the answer is not yet, then the next stage should focus on reliability, usability and polish rather than constantly adding new features.

The question should begin changing from:

> What else can Showduino do?

To:

> What can be simplified, removed or made impossible to misunderstand?

Every extra setting, mode and button becomes something a future operator must learn and something that can fail during a live show.

## Architecture review goals

The review should challenge the design without automatically rewriting it.

### Core roles

- Is the ESP32-P4 definitely in the correct authoritative role?
- Is the Director touchscreen correctly limited to control and presentation?
- Can the show continue safely if the Director disconnects?
- Is there one clear owner for every important piece of state?

### Communications

- Is ESP-NOW used only where it provides a real advantage?
- Should Ethernet or wired links carry more of the heavy traffic?
- Are acknowledgements, retries and timeouts consistent?
- Can duplicate, delayed or out-of-order packets cause unsafe behaviour?
- Are packet formats versioned and future-proof?

### Safety

- Does STOP override every subsystem?
- Is STOP latched and difficult to clear accidentally?
- What happens when a node freezes, reboots or disappears?
- What does each processor do when communication is lost?
- Are outputs left in a known safe state after a crash or restart?

### Show engine

- Is show state deterministic?
- Can a show resume safely after interruption?
- Are timelines separated from hardware drivers?
- Can audio, lighting, relays and effects stay synchronised?
- Are manual overrides predictable and reversible?

### Maintainability

- Could another developer understand the project structure?
- Are board-specific details separated from shared logic?
- Are protocols and state machines documented?
- Are names consistent across processors?
- Can individual components be tested without the complete system?

### User experience

- Can a user create a show without editing source code?
- Are errors explained in plain language?
- Can the operator tell which devices are online, degraded or unsafe?
- Is the interface usable under pressure during a live event?
- Are advanced settings hidden until needed?

### Product trust

- What would make a theatre, scare attraction or installation operator trust Showduino?
- Can it recover from power loss?
- Can configuration be backed up and restored?
- Can updates be performed without breaking existing shows?
- Is there a clear compatibility policy for future versions?

## Definition of a strong v1

A strong v1 does not need every planned feature.

It needs to:

- start reliably
- discover the expected hardware
- report faults clearly
- run a complete show deterministically
- stop safely
- recover cleanly
- preserve configuration
- be understandable without reading the code

## Recommended direction

Do not begin with a rewrite.

First:

1. Document the current architecture exactly as it exists.
2. Identify the authoritative owner of every state and output.
3. Define failure behaviour for every communication link.
4. Freeze the wire protocol for v1 unless a serious flaw is found.
5. Test full show operation with deliberate failures introduced.
6. Remove or postpone features that weaken reliability or usability.
7. Create a clear v1 acceptance checklist.

The purpose of this review is not to prove that Showduino was built incorrectly.

It is to confirm which foundations are strong, expose any genuine weaknesses before release, and give confidence that the project is ready to move from passion project to dependable product.
