#ifndef SHOWDUINO_WEB_TUNNEL_H
#define SHOWDUINO_WEB_TUNNEL_H

/*
 * Showduino Studio — HTTP tunnel over C3↔P4 UART (Stage 4+).
 *
 * Does NOT modify ESP-NOW desk/node packet sizes or colon-text show commands.
 * Distinct prefix so line-oriented parsers can ignore tunnel traffic until framed.
 *
 * Request (C3 → P4, newline-terminated ASCII):
 *   WEB/GET/api/system
 *   WEB/GET/api/devices
 *   WEB/GET/api/logs
 *
 * (Legacy double-slash WEB/GET//api/... is also accepted on P4.)
 *
 * Response (P4 → C3):
 *   WEBR:<status>:<bodyLen>\n
 *   <bodyLen bytes of JSON body — no newlines required>
 *
 * C3 Wi-Fi front door serves static Studio assets; /api/* is proxied via this tunnel.
 */

#define SHOWDUINO_WEB_TUNNEL_REQ_PREFIX "WEB/"
#define SHOWDUINO_WEB_TUNNEL_RESP_PREFIX "WEBR:"

#define SHOWDUINO_WEB_TUNNEL_BODY_MAX 4096u

#endif /* SHOWDUINO_WEB_TUNNEL_H */
