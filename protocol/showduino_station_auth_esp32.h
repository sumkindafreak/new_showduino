#ifndef SHOWDUINO_STATION_AUTH_ESP32_H
#define SHOWDUINO_STATION_AUTH_ESP32_H

/*
 * MISSION 001J: ESP32 bindings for protocol/showduino_station_auth.h.
 *   HMAC-SHA256 : mbedtls (part of ESP-IDF, used by the Arduino-ESP32 core).
 *   Randomness  : esp_fill_random(). On the C3 this is a true RNG while Wi-Fi runs.
 *                 On the P4 (no radio) it is only true random while the SAR ADC
 *                 entropy source is enabled, so the P4 seeds a ShowduinoNonceGen
 *                 once at boot (see EmergencyNodeLink) instead of using it directly.
 * Firmware only. The host tests use OpenSSL with the same function types.
 */

#include <stddef.h>
#include <stdint.h>
#include <mbedtls/md.h>
#include <esp_random.h>
#include "showduino_station_auth.h"

static inline int showduino_auth_hmac_sha256_mbedtls(const uint8_t *key, size_t keyLen,
                                                     const uint8_t *msg, size_t msgLen,
                                                     uint8_t out[32]) {
  const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (!info) return -1;
  return mbedtls_md_hmac(info, key, keyLen, msg, msgLen, out) == 0 ? 0 : -1;
}

static inline int showduino_auth_rand_esp32(uint8_t *out, size_t n) {
  if (!out) return -1;
  esp_fill_random(out, n);
  return 0;
}

#endif
