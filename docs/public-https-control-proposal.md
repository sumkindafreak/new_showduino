# Proposal: public HTTPS website → local Showduino control

```text
Status: PROPOSAL ONLY — not implemented
Local SoftAP control: docs/browser-control.md (implemented)
```

## Problem

A publicly hosted HTTPS site (e.g. `https://showduino.com`) **cannot** safely open sockets to a venue SoftAP at `http://192.168.4.1/` from a visitor's browser:

- Mixed content / private-network access restrictions
- No stable public IP for the SoftAP
- Permissive CORS does **not** create a secure path and must not be used as a substitute for authentication
- Port-forwarding the Communications Controller to the internet is out of scope and unsafe

Local SoftAP HTTP access therefore does **not** provide internet control.

## Recommended directions (pick one later)

### A. Outbound authenticated gateway (preferred)

Venue Comms (or a small companion service on the LAN) maintains an **outbound** TLS connection to a Showduino cloud relay. The public website talks only to that relay. Commands are forwarded inbound over the already-authenticated tunnel.

- P4 remains authority; cloud never invents show state
- No inbound port-forward to SoftAP
- Tokens / device pairing required; rotate credentials out-of-band

### B. Operator VPN / Tailscale-style mesh

Operator joins a private mesh that can reach SoftAP or a LAN HTTPS reverse proxy terminating TLS on-site. Website stays on the LAN view of the device.

### C. Export / import only

Public Studio authors SHDO packages for download; on-site SoftAP Studio or SD deploy installs them. No live remote start/stop over the public web.

## Explicit non-goals for a first public bridge

- Wildcard `Access-Control-Allow-Origin: *` with credentials
- Embedding SoftAP passwords in URLs or page query strings
- Auto-replay of action commands after reconnect
- Remote emergency CLEAR from the public internet
- Claiming the current SoftAP path is “cloud ready”

## Mapping to current local API

Any future gateway should preserve the local contract:

```text
POST /api/command  { "cmd", "requestId" }
lifecycle: accepted | rejected | duplicate
poll GET /api/show + GET /api/system for confirmation
```

Transport acknowledgements must not be presented as physical completion.

## Next implementation slice (when authorised)

1. Device pairing + short-lived command tokens  
2. Outbound WSS/MQTT from Comms with backpressure and offline queue policy (no silent replay of START)  
3. Public UI that disables controls when the venue tunnel is down  
4. Security review before any production venue enablement  
