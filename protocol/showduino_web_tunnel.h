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
 *
 * (Legacy double-slash WEB/GET//api/... is also accepted on P4.)
 *
 * Response (P4 → bridge):
 *   WEBR:<status>:<bodyLen>\n
 *   <bodyLen bytes of JSON body — no newlines required>
 *
 * Historical C3 firmware used this tunnel as a Wi-Fi HTTP radio.
 * The dedicated S3 Comms Controller does not host SoftAP in this phase;
 * P4 remains the origin (static files from SD /showduino/webui/ and JSON APIs).
 * Onboard C6 is unused reserved hardware.
 * Response header:
 *   WEBR:<status>:<bodyLen>[:<mime>]
 */

#define SHOWDUINO_WEB_TUNNEL_REQ_PREFIX "WEB/"
#define SHOWDUINO_WEB_TUNNEL_RESP_PREFIX "WEBR:"

#define SHOWDUINO_WEB_TUNNEL_BODY_MAX 24576u

#endif /* SHOWDUINO_WEB_TUNNEL_H */
