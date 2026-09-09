#ifndef SHOWDUINO_RADIO_H
#define SHOWDUINO_RADIO_H

/*
 * Deterministic Showduino radio policy.
 *
 * Comms S3 is the channel authority.
 *   STA associated  → operating channel = venue AP channel
 *   STA idle        → operating channel = SHOWDUINO_RADIO_HOME_CHANNEL
 * SoftAP and ESP-NOW always follow the operating channel.
 * Specialist nodes and the Director never pick a conflicting channel
 * independently; they follow the Showduino SoftAP SSID when unlinked.
 *
 * Do not deinitialise ESP-NOW, delete established peers, or restart
 * SoftAP merely because STA associated or a node joined.
 */

#ifndef SHOWDUINO_RADIO_HOME_CHANNEL
#ifdef SHOWDUINO_ESPNOW_CHANNEL
#define SHOWDUINO_RADIO_HOME_CHANNEL SHOWDUINO_ESPNOW_CHANNEL
#else
#define SHOWDUINO_RADIO_HOME_CHANNEL 1
#endif
#endif

#define SHOWDUINO_RADIO_CHANNEL_MIN 1
#define SHOWDUINO_RADIO_CHANNEL_MAX 13
#define SHOWDUINO_RADIO_AP_SSID "Showduino"
#define SHOWDUINO_RADIO_FOLLOW_BOOT_MS 3500UL
#define SHOWDUINO_RADIO_FOLLOW_RETRY_MIN_MS 5000UL
#define SHOWDUINO_RADIO_FOLLOW_RETRY_MAX_MS 20000UL
#define SHOWDUINO_RADIO_LINK_FRESH_MS 8000UL

#endif /* SHOWDUINO_RADIO_H */
