#ifndef SHOWDUINO_EMERGENCY_AUTH_H
#define SHOWDUINO_EMERGENCY_AUTH_H

/*
 * MISSION 001J: Emergency Node side of station authentication.
 *
 * The station key is created or entered on this board's USB console only:
 *   PROVISION NEW ESTOP-nn         generate a key (true RNG: Wi-Fi is running),
 *                                  set this station's ID, print the key ONCE
 *   PROVISION SET ESTOP-nn <hex64> use a key generated elsewhere
 *   PROVISION CLEAR                forget the key (factory reset of credentials)
 *   PROVISION STATUS               ID, provisioned yes/no, key fingerprint
 * All are refused while the button is pressed or the station is latched.
 * The same key is then entered on the P4 USB console (EMERGENCY:KEY:SET).
 *
 * The key is stored in NVS in plaintext (no flash encryption on this board).
 */

#include <Arduino.h>

void emergencyAuthBegin();
bool emergencyAuthProvisioned();
/* Call every service pass. Tracks the local latch for episode binding. */
void emergencyAuthService();
/* USB console. Returns true when the line was a PROVISION command. */
bool emergencyAuthHandleUsb(const char *line);
/* Radio CHAL / CMD. Returns true when the line was consumed. */
bool emergencyAuthHandleRadio(const char *line, uint32_t sequence);

#endif
