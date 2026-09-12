#include "LampAudio.h"
#include "LampConfig.h"
#include "../BoardConfig.h"

#include <Arduino.h>
#include <string.h>

/*
 * Local carbide-lamp effect audio via DFRobot Fermion DFPlayer Pro (DFR0768).
 * UART AT-command transport. Not a Showduino Audio Node. No BUSY pin.
 * Visual lamp behaviour must continue if the Fermion is absent.
 */

static bool sPinsOk = false;
static bool sReady = false;
static bool sBegun = false;
static uint8_t sVol = 18;
static ShowduinoLampSound sCurrent = SHOWDUINO_LAMP_SND_NONE;
static char sErr[24] = "-";
static uint32_t sLastCmdMs = 0;

static bool pinsConfirmed() {
  return SHOWDUINO_LAMP_FERMION_TX_PIN >= 0 && SHOWDUINO_LAMP_FERMION_RX_PIN >= 0;
}

static void sendAt(const char *line) {
  if (!sBegun || !line) return;
  Serial1.print(line);
  Serial1.print("\r\n");
  sLastCmdMs = millis();
}

void lampAudioBegin() {
  sVol = lampConfigAudioVolume();
  sCurrent = SHOWDUINO_LAMP_SND_NONE;
  sPinsOk = pinsConfirmed();
  sReady = false;
  sBegun = false;
  strncpy(sErr, sPinsOk ? "-" : "UNCONFIRMED", sizeof(sErr) - 1);
  sErr[sizeof(sErr) - 1] = 0;
  if (!sPinsOk) {
    Serial.println("[LAMP-AUD] Fermion UART pins UNCONFIRMED — visual-only");
    return;
  }
  Serial1.begin((unsigned long)SHOWDUINO_LAMP_FERMION_BAUD, SERIAL_8N1,
                SHOWDUINO_LAMP_FERMION_RX_PIN, SHOWDUINO_LAMP_FERMION_TX_PIN);
  sBegun = true;
  sendAt("AT");
  char vol[24];
  snprintf(vol, sizeof(vol), "AT+VOL=%u", (unsigned)sVol);
  sendAt(vol);
  sReady = true;
  strncpy(sErr, "-", sizeof(sErr) - 1);
  Serial.printf("[LAMP-AUD] Fermion UART TX=%d RX=%d baud=%lu (local FX only)\n",
                SHOWDUINO_LAMP_FERMION_TX_PIN, SHOWDUINO_LAMP_FERMION_RX_PIN,
                (unsigned long)SHOWDUINO_LAMP_FERMION_BAUD);
}

void lampAudioService() {
  if (!sBegun) return;
  while (Serial1.available() > 0) (void)Serial1.read();
}

void lampAudioPlay(ShowduinoLampSound id) {
  sCurrent = id;
  if (id == SHOWDUINO_LAMP_SND_NONE) {
    lampAudioStop();
    return;
  }
  if (!sBegun) return;
  const ShowduinoLampSoundMap *info = showduino_lamp_sound_info(id);
  if (!info || !info->file[0]) return;
  sendAt("AT+STOP");
  char line[48];
  snprintf(line, sizeof(line), "AT+PLAYFILE=%s", info->file);
  sendAt(line);
  if (info->loop) sendAt("AT+PLAYMODE=2");
}

void lampAudioStop() {
  sCurrent = SHOWDUINO_LAMP_SND_NONE;
  if (sBegun) sendAt("AT+STOP");
}

bool lampAudioHardwarePresent() { return sPinsOk; }
bool lampAudioReady() { return sReady; }

const char *lampAudioStatus() {
  if (!sPinsOk) return "UNCONFIRMED";
  if (!sReady) return "FAULT";
  return "OK";
}

const char *lampAudioCurrentRole() {
  return showduino_lamp_sound_info(sCurrent)->role;
}

void lampAudioSetVolume(uint8_t vol) {
  if (vol > 30) vol = 30;
  sVol = vol;
  lampConfigSetAudioVolume(vol);
  if (sBegun) {
    char line[24];
    snprintf(line, sizeof(line), "AT+VOL=%u", (unsigned)sVol);
    sendAt(line);
  }
}

uint8_t lampAudioVolume() { return sVol; }
const char *lampAudioLastError() { return sErr; }
