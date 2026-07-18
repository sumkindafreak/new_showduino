# Showduino Constitution

Permanent architectural rules. Implementation maturity is documented elsewhere (`docs/architecture.md`, `docs/repository-status.md`). This file defines what must remain true as the platform evolves.

---

## Article I — One source of truth

The Show Engine is the sole authoritative source of:

* Show state
* Timeline state
* Cue state
* Project state
* Node state known by the system
* Output state known by the system
* Fault state
* Emergency state

Other components may display, cache, or locally enforce state, but they must not independently redefine authoritative show state.

---

## Article II — The Show Engine decides

The Show Engine:

* Validates requests
* Owns state transitions
* Executes shows
* Publishes authoritative state
* Applies safety policy
* Coordinates nodes and local outputs

The current Show Engine hardware is the ESP32-P4 Stage Controller.

---

## Article III — The Communications Engine transports

The Communications Engine:

* Provides ESP-NOW
* Provides Wi-Fi
* Provides UART transport
* Routes messages
* Tracks communication health
* Maps logical device IDs to transport addresses

It must not:

* Run timelines
* Make cue decisions
* Change authoritative show state
* Independently start or stop shows
* Claim that physical actions completed without confirmation

The current Communications Engine hardware is the ESP32-C3.

---

## Article IV — The Director commands and displays

The Director:

* Presents the operator interface
* Sends requests
* Displays confirmed state
* Displays warnings and faults
* Provides emergency controls

Director actions are requests.

The Director must not display a successful authoritative state change solely because a request was sent.

The current Director hardware is the ESP32-S3 touchscreen.

---

## Article V — Nodes act

Nodes perform specialist local actions.

Examples include:

* Relay control
* Audio playback
* Lighting control
* Sensor monitoring
* Motor control
* Animatronic control
* Environmental effects

Nodes may enforce local safety behaviour but do not own the overall show state.

---

## Article VI — Transport is not meaning

Application messages must describe intent.

ESP-NOW, UART, Wi-Fi, USB, Ethernet, and other links are transports.

Transport-specific terminology must not define application behaviour.

Application logic must use stable Showduino device IDs rather than MAC addresses or physical link identifiers.

---

## Article VII — Commands are requests

A command lifecycle may include:

1. Request created
2. Request received
3. Request accepted or rejected
4. Request forwarded
5. Node receipt acknowledged
6. Action started
7. Action completed or failed
8. Authoritative state changed

Command acceptance must not automatically be treated as physical completion.

---

## Article VIII — State is published

Whenever authoritative state changes, the Show Engine publishes the resulting state.

Interfaces must rebuild their displays from confirmed state and state snapshots rather than assumptions.

---

## Article IX — Shows run locally

Once a show starts, its normal execution must not depend on:

* The Director remaining connected
* A browser remaining open
* Wi-Fi remaining available
* Internet access
* A cloud service

Loss of an operator interface must not erase or transfer authoritative state.

---

## Article X — Graceful degradation

The system must respond predictably to communication and device loss.

Examples:

* Director lost: running show continues
* Browser lost: running show continues
* Wi-Fi lost: local show execution continues
* Node lost: fault or configured fallback is applied
* Communications Engine lost: Show Engine follows configured safety policy
* Reconnection: state is restored through synchronisation

---

## Article XI — Safety overrides entertainment

Priority order:

```text
Emergency
Fault response
Manual safety control
Show timeline
Testing and diagnostics
```

No entertainment command may override an active emergency or safety lock.

---

## Article XII — Honest capability reporting

Documentation and interfaces must distinguish between:

* Implemented
* Partially implemented
* Planned
* Experimental
* Unsupported

Placeholder systems must not return false successful completion.

---

## Article XIII — Refactor before rewrite

Working systems should be preserved where practical.

Architectural improvements should be staged, testable, and reversible.

Large rewrites require clear evidence that incremental refactoring cannot safely achieve the same result.

---

> The Show Engine decides.  
> The Communications Engine transports.  
> The Director commands and displays.  
> The Nodes act.
