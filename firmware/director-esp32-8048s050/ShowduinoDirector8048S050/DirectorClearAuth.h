#ifndef SHOWDUINO_DIRECTOR_CLEAR_AUTH_H
#define SHOWDUINO_DIRECTOR_CLEAR_AUTH_H

/*
 * MISSION 001K: the Director's operator-clear credential.
 *
 * The Director signs the emergency clear request (touch "E-CLEAR") and the clear
 * confirm (touch "Confirm" in the clear dialog) with its own 32-byte key, end to end
 * to the P4 (protocol/showduino_director_auth.h). The Comms S3 relays the signed
 * lines and cannot produce one.
 *
 * Enrolment, on THIS board's USB console (115200, CH340 port) only:
 *   PROVISION DIRECTOR NEW DIR-01          generate a key (true RNG: Wi-Fi is running)
 *                                          and print the P4 line once
 *   PROVISION DIRECTOR SET DIR-01 <hex64>  use a key generated elsewhere
 *   PROVISION DIRECTOR STATUS              ID, fingerprint, counter
 *   PROVISION DIRECTOR CLEAR               forget the key
 * PROVISION lines are handled before anything else and are never forwarded or
 * logged. The key is stored in NVS without flash encryption.
 *
 * The Director is not the latch authority: the P4 decides, and a Director restart
 * or disconnect never releases an emergency.
 */

#include <Arduino.h>

void directorClearAuthBegin();
bool directorClearAuthProvisioned();
/* USB console. Returns true when the line was a PROVISION command (never forward it). */
bool directorClearAuthHandleUsb(const String &line);
/* Every line from the Stage: learns the episode and the clear transaction nonce. */
void directorClearAuthOnStageLine(const String &line);
/* Signs a clear request / confirm. On false, `why` is an operator-readable reason. */
bool directorClearAuthSignRequest(String &out, String &why);
bool directorClearAuthSignConfirm(String &out, String &why);

#endif
