#include "LampAudio.h"
#include "LampConfig.h"
#include "../BoardConfig.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

/*
 * Local carbide-lamp effect audio via Adafruit Audio FX Sound Board.
 * UART named-playback transport at 9600 8N1. Not a Showduino Audio Node.
 * Visual lamp behaviour must continue if the Audio FX board is absent or a
 * file is missing. Flame ambience restarts on "done" while BURN_LOOP is set.
 */

#define LAMP_AUD_BOOT_MS     800
#define LAMP_AUD_LIST_GAP_MS 40
#define LAMP_AUD_TX_Q        6
#define LAMP_AUD_TX_LEN      16
#define LAMP_AUD_RX_LEN      96

typedef struct {
  char line[LAMP_AUD_TX_LEN];
  uint8_t raw; /* 1 = send bytes as-is (stop 'q'), 0 = line + optional handling */
} LampAudTx;

static bool sPinsOk = false;
static bool sReady = false;
static bool sBegun = false;
static bool sListSent = false;
static bool sPlaying = false;
static uint8_t sHeard = 0;
static uint8_t sExpectedSeen = 0;
static ShowduinoLampSound sCurrent = SHOWDUINO_LAMP_SND_NONE;
static char sErr[32] = "-";
static char sLastRx[48] = "-";
static char sCurrentFat[SHOWDUINO_LAMP_AUDIO_FX_FAT_LEN + 1] = "";
static uint32_t sBootUntil = 0;
static uint32_t sNextSend = 0;
static LampAudTx sQ[LAMP_AUD_TX_Q];
static uint8_t sQh = 0;
static uint8_t sQn = 0;
static char sRx[LAMP_AUD_RX_LEN];
static uint8_t sRxN = 0;

static bool pinsConfirmed() {
  return SHOWDUINO_LAMP_AUDIO_FX_TX_PIN >= 0 &&
         SHOWDUINO_LAMP_AUDIO_FX_RX_PIN >= 0;
}

static void flushTx() {
  sQh = 0;
  sQn = 0;
}

static void enqueueRaw(const char *bytes) {
  LampAudTx *slot;
  if (!sBegun || !bytes || !bytes[0] || sQn >= LAMP_AUD_TX_Q) return;
  slot = &sQ[(sQh + sQn) % LAMP_AUD_TX_Q];
  strncpy(slot->line, bytes, LAMP_AUD_TX_LEN - 1);
  slot->line[LAMP_AUD_TX_LEN - 1] = 0;
  slot->raw = 1;
  sQn++;
}

static void enqueueLine(const char *line) {
  LampAudTx *slot;
  if (!sBegun || !line || !line[0] || sQn >= LAMP_AUD_TX_Q) return;
  slot = &sQ[(sQh + sQn) % LAMP_AUD_TX_Q];
  strncpy(slot->line, line, LAMP_AUD_TX_LEN - 1);
  slot->line[LAMP_AUD_TX_LEN - 1] = 0;
  slot->raw = 0;
  sQn++;
}

static bool enqueuePlayFile(const char *fat83Name) {
  char cmd[16];
  if (showduino_lamp_audio_fx_play_cmd(fat83Name, cmd, sizeof(cmd)) != 0) {
    strncpy(sErr, "BAD_FAT_NAME", sizeof(sErr) - 1);
    sErr[sizeof(sErr) - 1] = 0;
    return false;
  }
  if (!sBegun) {
    strncpy(sErr, sPinsOk ? "NO_UART" : "UNCONFIRMED", sizeof(sErr) - 1);
    sErr[sizeof(sErr) - 1] = 0;
    return false;
  }
  enqueueLine(cmd);
  strncpy(sCurrentFat, fat83Name, SHOWDUINO_LAMP_AUDIO_FX_FAT_LEN);
  sCurrentFat[SHOWDUINO_LAMP_AUDIO_FX_FAT_LEN] = 0;
  sPlaying = true;
  return true;
}

static void enqueueStop() {
  char cmd[4];
  if (!sBegun) return;
  if (showduino_lamp_audio_fx_stop_cmd(cmd, sizeof(cmd)) != 0) return;
  enqueueRaw(cmd);
  sPlaying = false;
  Serial.println("[LAMP-AUD] STOP");
}

static void sendQueued(const LampAudTx *slot) {
  if (!slot) return;
  if (slot->raw) {
    Serial1.print(slot->line);
    Serial.printf("[LAMP-AUD] TX %s\n", slot->line);
  } else {
    /* Play/list commands already include trailing '\n' when required. */
    Serial1.print(slot->line);
    if (slot->line[0] == 'L' && slot->line[1] == 0) {
      Serial1.print('\n');
      Serial.println("[LAMP-AUD] TX L");
    } else if (slot->line[0] == 'P') {
      Serial.printf("[LAMP-AUD] PLAY %s\n",
                    sCurrent != SHOWDUINO_LAMP_SND_NONE
                        ? showduino_lamp_sound_info(sCurrent)->file
                        : sCurrentFat);
    } else {
      Serial.printf("[LAMP-AUD] TX %s", slot->line);
    }
  }
  sNextSend = millis() + LAMP_AUD_LIST_GAP_MS;
}

static void pumpTx(uint32_t now) {
  if (!sBegun) return;
  if (!sListSent && now >= sBootUntil) {
    sListSent = true;
    enqueueLine("L");
  }
  if (!sQn || now < sBootUntil || now < sNextSend) return;
  sendQueued(&sQ[sQh]);
  sQh = (uint8_t)((sQh + 1) % LAMP_AUD_TX_Q);
  sQn--;
}

static void noteExpectedFile(const char *line) {
  size_t i;
  for (i = 1; i < SHOWDUINO_LAMP_SOUND_TABLE_LEN; ++i) {
    const char *fat = SHOWDUINO_LAMP_SOUND_TABLE[i].fat;
    if (fat && fat[0] && strncmp(line, fat, SHOWDUINO_LAMP_AUDIO_FX_FAT_LEN) == 0) {
      sExpectedSeen = (uint8_t)(sExpectedSeen | (1u << (i - 1)));
    }
  }
}

static void onRxLine(const char *line) {
  if (!line || !line[0]) return;
  strncpy(sLastRx, line, sizeof(sLastRx) - 1);
  sLastRx[sizeof(sLastRx) - 1] = 0;
  sHeard = 1;
  sReady = true;

  if (strncmp(line, "play", 4) == 0) {
    sPlaying = true;
    Serial.printf("[LAMP-AUD] RX %s\n", line);
    return;
  }
  if (strcmp(line, "done") == 0) {
    Serial.println("[LAMP-AUD] RX done");
    sPlaying = false;
    if (showduino_lamp_audio_fx_should_restart_on_done(sCurrent)) {
      const char *fat = showduino_lamp_sound_fat(sCurrent);
      if (fat[0] && enqueuePlayFile(fat)) {
        Serial.printf("[LAMP-AUD] LOOP restart %s\n",
                      showduino_lamp_sound_info(sCurrent)->file);
      }
    }
    return;
  }
  if (strncmp(line, "NoFile", 6) == 0) {
    snprintf(sErr, sizeof(sErr), "NO_FILE: %s",
             sCurrentFat[0] ? sCurrentFat : "-");
    sPlaying = false;
    Serial.printf("[LAMP-AUD] ERROR NoFile (%s)\n", sErr);
    return;
  }
  noteExpectedFile(line);
  Serial.printf("[LAMP-AUD] RX %s\n", line);
}

static void drainRx() {
  while (Serial1.available() > 0) {
    const char c = (char)Serial1.read();
    if (c == '\r') continue;
    if (c == '\n') {
      sRx[sRxN] = 0;
      if (sRxN > 0) onRxLine(sRx);
      sRxN = 0;
      continue;
    }
    if (sRxN + 1U < sizeof(sRx)) sRx[sRxN++] = c;
  }
}

void lampAudioBegin() {
  sCurrent = SHOWDUINO_LAMP_SND_NONE;
  sPinsOk = pinsConfirmed();
  sReady = false;
  sBegun = false;
  sListSent = false;
  sPlaying = false;
  sHeard = 0;
  sExpectedSeen = 0;
  sCurrentFat[0] = 0;
  sRxN = 0;
  strncpy(sLastRx, "-", sizeof(sLastRx) - 1);
  strncpy(sErr, sPinsOk ? "-" : "UNCONFIRMED", sizeof(sErr) - 1);
  sErr[sizeof(sErr) - 1] = 0;
  flushTx();
  if (!sPinsOk) {
    Serial.println("[LAMP-AUD] Audio FX UART pins UNCONFIRMED — visual-only");
    return;
  }
  Serial1.begin((unsigned long)SHOWDUINO_LAMP_AUDIO_FX_BAUD, SERIAL_8N1,
                SHOWDUINO_LAMP_AUDIO_FX_RX_PIN, SHOWDUINO_LAMP_AUDIO_FX_TX_PIN);
  sBegun = true;
  sBootUntil = millis() + LAMP_AUD_BOOT_MS;
  sNextSend = sBootUntil;
  sReady = true;
  strncpy(sErr, "-", sizeof(sErr) - 1);
  Serial.printf("[LAMP-AUD] Audio FX UART started %lu (TX=%d RX=%d)\n",
                (unsigned long)SHOWDUINO_LAMP_AUDIO_FX_BAUD,
                SHOWDUINO_LAMP_AUDIO_FX_TX_PIN, SHOWDUINO_LAMP_AUDIO_FX_RX_PIN);
  Serial.println("[LAMP-AUD] V1 files: flick.wav fire_ign.wav flameloo.wav emergency.wav");
  Serial.println("[LAMP-AUD] FAT: FLICK   WAV / FIRE_IGNWAV / FLAMELOOWAV / EMERGENCWAV");
}

void lampAudioService() {
  if (!sBegun) return;
  drainRx();
  pumpTx(millis());
}

void lampAudioPlay(ShowduinoLampSound id) {
  const ShowduinoLampSoundMap *info;
  if (id == SHOWDUINO_LAMP_SND_NONE) {
    lampAudioStop();
    return;
  }
  info = showduino_lamp_sound_info(id);
  if (!info || !info->fat || !info->fat[0]) {
    lampAudioStop();
    return;
  }
  /* Stop current WAV so strike→ignition and emergency interrupt cleanly. */
  if (sBegun) {
    flushTx();
    enqueueStop();
  }
  sCurrent = id;
  if (!enqueuePlayFile(info->fat)) return;
  strncpy(sErr, "-", sizeof(sErr) - 1);
  sErr[sizeof(sErr) - 1] = 0;
}

void lampAudioStop() {
  sCurrent = SHOWDUINO_LAMP_SND_NONE;
  sCurrentFat[0] = 0;
  if (!sBegun) return;
  flushTx();
  enqueueStop();
}

bool lampAudioHardwarePresent() { return sPinsOk; }
bool lampAudioReady() { return sReady; }
bool lampAudioHeardReply() { return sHeard != 0; }
bool lampAudioPlaying() { return sPlaying; }

const char *lampAudioStatus() {
  if (!sPinsOk) return "UNCONFIRMED";
  if (!sReady) return "FAULT";
  if (!sListSent || millis() < sBootUntil) return "STARTING";
  if (!sHeard) return "NO_REPLY";
  return "OK";
}

const char *lampAudioModuleName() { return "ADAFRUIT AUDIO FX"; }

const char *lampAudioCurrentRole() {
  return showduino_lamp_sound_info(sCurrent)->role;
}

const char *lampAudioCurrentFile() {
  return showduino_lamp_sound_info(sCurrent)->file;
}

const char *lampAudioCurrentFat() {
  return sCurrentFat[0] ? sCurrentFat : showduino_lamp_sound_fat(sCurrent);
}

const char *lampAudioFileQueryStatus() {
  return SHOWDUINO_LAMP_FILE_QUERY_STATUS;
}

const char *lampAudioExpectedFiles() {
  return "flick.wav,fire_ign.wav,flameloo.wav,emergency.wav";
}

const char *lampAudioLastRx() { return sLastRx; }

void lampAudioSetVolume(uint8_t /*vol*/) {
  /* Audio FX uses +/- relative volume; external amp is authoritative. */
  strncpy(sErr, "AMP_GAIN", sizeof(sErr) - 1);
  sErr[sizeof(sErr) - 1] = 0;
}

uint8_t lampAudioVolume() { return 0; }

const char *lampAudioVolumeNote() {
  return "AMP GAIN AUTHORITATIVE";
}

const char *lampAudioLastError() { return sErr; }
