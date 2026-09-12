#include "LampAudio.h"
#include "LampConfig.h"
#include "../BoardConfig.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

/*
 * Local carbide-lamp effect audio via DFRobot Fermion DFPlayer Pro (DFR0768).
 * UART AT-command transport. Not a Showduino Audio Node. No BUSY pin.
 * Visual lamp behaviour must continue if the Fermion is absent or a file
 * is missing. File enumeration is unsupported on this AT path.
 *
 * Official init (non-blocking): wait for module boot, AT, FUNCTION=1 (MUSIC),
 * AMP=ON, VOL. Play uses PLAYFILE=/name.mp3 and wiki PLAYMODE 1/3.
 * Commands are queued with a gap — the module drops a burst.
 */

#define LAMP_AT_Q          8
#define LAMP_AT_LEN        40
#define LAMP_AT_GAP_MS     80
#define LAMP_AT_BOOT_MS    1200
#define LAMP_AT_FUNC_MS    1600

typedef struct {
  char line[LAMP_AT_LEN];
  uint16_t gapMs;
} LampAtCmd;

static bool sPinsOk = false;
static bool sReady = false;
static bool sBegun = false;
static bool sInitDone = false;
static uint8_t sVol = 18;
static ShowduinoLampSound sCurrent = SHOWDUINO_LAMP_SND_NONE;
static char sErr[24] = "-";
static char sLastRx[48] = "-";
static uint8_t sHeard = 0;
static uint32_t sLastCmdMs = 0;
static uint32_t sBootUntil = 0;
static uint32_t sNextSend = 0;
static LampAtCmd sQ[LAMP_AT_Q];
static uint8_t sQh = 0;
static uint8_t sQn = 0;

static bool pinsConfirmed() {
  return SHOWDUINO_LAMP_FERMION_TX_PIN >= 0 && SHOWDUINO_LAMP_FERMION_RX_PIN >= 0;
}

static void enqueueAt(const char *line, uint16_t gapMs) {
  LampAtCmd *slot;
  if (!sBegun || !line || !line[0] || sQn >= LAMP_AT_Q) return;
  slot = &sQ[(sQh + sQn) % LAMP_AT_Q];
  strncpy(slot->line, line, LAMP_AT_LEN - 1);
  slot->line[LAMP_AT_LEN - 1] = 0;
  slot->gapMs = gapMs ? gapMs : LAMP_AT_GAP_MS;
  sQn++;
}

static void flushAt() {
  sQh = 0;
  sQn = 0;
}

static void sendAtNow(const char *line) {
  if (!sBegun || !line) return;
  Serial1.print(line);
  Serial1.print("\r\n");
  sLastCmdMs = millis();
  Serial.printf("[LAMP-AUD] TX %s\n", line);
}

static void drainRx() {
  static char rx[96];
  static uint8_t n = 0;
  while (Serial1.available() > 0) {
    const char c = (char)Serial1.read();
    if (c == '\r') continue;
    if (c == '\n') {
      rx[n] = 0;
      if (n > 0) {
        strncpy(sLastRx, rx, sizeof(sLastRx) - 1);
        sLastRx[sizeof(sLastRx) - 1] = 0;
        if (strstr(rx, "OK")) sHeard = 1;
        Serial.printf("[LAMP-AUD] RX %s\n", rx);
      }
      n = 0;
      continue;
    }
    if (n + 1U < sizeof(rx)) rx[n++] = c;
  }
}

static void pumpAt(uint32_t now) {
  if (!sBegun || !sQn) {
    if (sBegun && !sInitDone && now >= sBootUntil) sInitDone = true;
    return;
  }
  if (now < sBootUntil || now < sNextSend) return;
  sendAtNow(sQ[sQh].line);
  sNextSend = now + sQ[sQh].gapMs;
  sQh = (uint8_t)((sQh + 1) % LAMP_AT_Q);
  sQn--;
  if (!sQn) sInitDone = true;
}

void lampAudioBegin() {
  char vol[24];
  sVol = lampConfigAudioVolume();
  sCurrent = SHOWDUINO_LAMP_SND_NONE;
  sPinsOk = pinsConfirmed();
  sReady = false;
  sBegun = false;
  sInitDone = false;
  sHeard = 0;
  strncpy(sLastRx, "-", sizeof(sLastRx) - 1);
  strncpy(sErr, sPinsOk ? "-" : "UNCONFIRMED", sizeof(sErr) - 1);
  sErr[sizeof(sErr) - 1] = 0;
  flushAt();
  if (!sPinsOk) {
    Serial.println("[LAMP-AUD] Fermion UART pins UNCONFIRMED — visual-only");
    return;
  }
  Serial1.begin((unsigned long)SHOWDUINO_LAMP_FERMION_BAUD, SERIAL_8N1,
                SHOWDUINO_LAMP_FERMION_RX_PIN, SHOWDUINO_LAMP_FERMION_TX_PIN);
  sBegun = true;
  sBootUntil = millis() + LAMP_AT_BOOT_MS;
  sNextSend = sBootUntil;
  enqueueAt("AT", LAMP_AT_GAP_MS);
  enqueueAt("AT+FUNCTION=1", LAMP_AT_FUNC_MS);
  enqueueAt("AT+AMP=ON", LAMP_AT_GAP_MS);
  snprintf(vol, sizeof(vol), "AT+VOL=%u", (unsigned)sVol);
  enqueueAt(vol, LAMP_AT_GAP_MS);
  enqueueAt("AT+PROMPT=OFF", LAMP_AT_GAP_MS);
  sReady = true;
  strncpy(sErr, "-", sizeof(sErr) - 1);
  Serial.printf("[LAMP-AUD] Fermion UART TX=%d RX=%d baud=%lu (local FX only)\n",
                SHOWDUINO_LAMP_FERMION_TX_PIN, SHOWDUINO_LAMP_FERMION_RX_PIN,
                (unsigned long)SHOWDUINO_LAMP_FERMION_BAUD);
  Serial.println("[LAMP-AUD] V1 files: flick.mp3 fire_ignite.mp3 flameloop.mp3 emergency.mp3");
  Serial.println("[LAMP-AUD] File query UNSUPPORTED — not inventing present/missing");
  Serial.println("[LAMP-AUD] Init queued: AT, FUNCTION=MUSIC, AMP=ON, VOL, PROMPT=OFF");
}

void lampAudioService() {
  if (!sBegun) return;
  drainRx();
  pumpAt(millis());
}

void lampAudioPlay(ShowduinoLampSound id) {
  char cmd[40];
  const ShowduinoLampSoundMap *info;
  sCurrent = id;
  if (id == SHOWDUINO_LAMP_SND_NONE) {
    lampAudioStop();
    return;
  }
  info = showduino_lamp_sound_info(id);
  if (!info || !info->file[0]) {
    if (sBegun && sInitDone) flushAt();
    if (sBegun) enqueueAt("AT+STOP", LAMP_AT_GAP_MS);
    return;
  }
  if (!sBegun) {
    strncpy(sErr, sPinsOk ? "NO_UART" : "UNCONFIRMED", sizeof(sErr) - 1);
    sErr[sizeof(sErr) - 1] = 0;
    return;
  }
  if (sInitDone) flushAt();
  snprintf(cmd, sizeof(cmd), "AT+PLAYMODE=%u",
           (unsigned)showduino_lamp_fermion_playmode(info->loop));
  enqueueAt(cmd, LAMP_AT_GAP_MS);
  if (showduino_lamp_fermion_playfile_cmd(info->file, cmd, sizeof(cmd)) == 0) {
    enqueueAt(cmd, LAMP_AT_GAP_MS);
  }
}

void lampAudioStop() {
  sCurrent = SHOWDUINO_LAMP_SND_NONE;
  if (!sBegun) return;
  if (sInitDone) flushAt();
  enqueueAt("AT+STOP", LAMP_AT_GAP_MS);
}

bool lampAudioHardwarePresent() { return sPinsOk; }
bool lampAudioReady() { return sReady; }
bool lampAudioHeardReply() { return sHeard != 0; }

const char *lampAudioStatus() {
  if (!sPinsOk) return "UNCONFIRMED";
  if (!sReady) return "FAULT";
  if (!sInitDone) return "STARTING";
  if (!sHeard) return "NO_REPLY";
  return "OK";
}

const char *lampAudioCurrentRole() {
  return showduino_lamp_sound_info(sCurrent)->role;
}

const char *lampAudioCurrentFile() {
  return showduino_lamp_sound_info(sCurrent)->file;
}

const char *lampAudioFileQueryStatus() {
  return SHOWDUINO_LAMP_FILE_QUERY_STATUS;
}

const char *lampAudioExpectedFiles() {
  return "flick.mp3,fire_ignite.mp3,flameloop.mp3,emergency.mp3";
}

const char *lampAudioLastRx() { return sLastRx; }

void lampAudioSetVolume(uint8_t vol) {
  char line[24];
  if (vol > 30) vol = 30;
  sVol = vol;
  lampConfigSetAudioVolume(vol);
  if (!sBegun) return;
  snprintf(line, sizeof(line), "AT+VOL=%u", (unsigned)sVol);
  enqueueAt(line, LAMP_AT_GAP_MS);
}

uint8_t lampAudioVolume() { return sVol; }
const char *lampAudioLastError() { return sErr; }
