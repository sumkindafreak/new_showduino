#ifndef SHOWDUINO_WEB_TUNNEL_H
#define SHOWDUINO_WEB_TUNNEL_H

/*
 * Showduino Studio — HTTP tunnel over Communications Engine ↔ P4 UART.
 *
 * Does NOT modify ESP-NOW desk/node packet sizes or colon-text show commands.
 * Distinct prefix so line-oriented parsers can ignore tunnel traffic until framed.
 *
 * Request (bridge → P4, newline-terminated ASCII):
 *   WEB/GET/api/system
 *   WEB/GET/api/devices
 *   WEB/GET/api/logs
 *   WEB/POST/api/command/SHOW:START
 *
 * (Legacy double-slash WEB/GET//api/... is also accepted on P4.)
 *
 * Response (P4 → bridge):
 *   WEBR:<status>:<bodyLen>\n
 *   <bodyLen bytes of JSON body — no newlines required>
 *
 * Canonical host: Communications S3 serves PROGMEM static WebUI and proxies
 * GET /api/system, /api/logs, /api/devices over this UART tunnel.
 * P4 remains the API origin and must not invent show state on the S3.
 * One in-flight GET at a time (WebServer.handleClient is serial).
 * Bodies are length-prefixed (not newline-framed) up to BODY_MAX.
 * Onboard C6 is unused reserved hardware.
 * Response header:
 *   WEBR:<status>:<bodyLen>[:<mime>]
 */

#define SHOWDUINO_WEB_TUNNEL_REQ_PREFIX "WEB/"
#define SHOWDUINO_WEB_TUNNEL_RESP_PREFIX "WEBR:"

#define SHOWDUINO_WEB_TUNNEL_BODY_MAX 24576u

#endif /* SHOWDUINO_WEB_TUNNEL_H */
