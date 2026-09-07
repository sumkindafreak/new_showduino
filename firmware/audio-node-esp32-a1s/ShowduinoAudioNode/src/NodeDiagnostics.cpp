#include "NodeDiagnostics.h"
#include "AudioCodec.h"
#include "AudioCommand.h"
#include "AudioPlayback.h"
#include "AudioStorage.h"
#include "AudioNodeState.h"
#include "EspNowNodeTransport.h"
#include "LocalButtons.h"
#include "input/AudioInput.h"
#include "../BoardConfig.h"
#include "../../../protocol/showduino_audio_node.h"

static uint32_t sLedTestUntil = 0;
static uint32_t sLoopUs = 0;
static uint32_t sLoopMaxUs = 0;
static uint32_t sMinHeap = 0;

static bool ledOnForState(uint32_t now) {
  const ShowduinoAudioNodeState st = audioNodeState();
  const bool haveComms = audioEspNowHaveComms();

  if (st == SHOWDUINO_AUDIO_ST_EMERGENCY) {
    return ((now / 70) & 1) != 0;
  }
  if (st == SHOWDUINO_AUDIO_ST_FAULT) {
    return ((now / 110) & 1) != 0;
  }
  if (st == SHOWDUINO_AUDIO_ST_NO_STORAGE) {
    const uint32_t phase = now % 1400;
    return phase < 120 || (phase > 200 && phase < 320) || (phase > 400 && phase < 520);
  }
  if (st == SHOWDUINO_AUDIO_ST_BOOTING) {
    return ((now / 700) & 1) != 0;
  }
  if (st == SHOWDUINO_AUDIO_ST_PAUSED) {
    return ((now / 900) & 1) != 0;
  }
  if (st == SHOWDUINO_AUDIO_ST_PLAYING || st == SHOWDUINO_AUDIO_ST_LOOPING ||
      st == SHOWDUINO_AUDIO_ST_LOADING || st == SHOWDUINO_AUDIO_ST_STOPPING) {
    return ((now / 180) & 1) != 0;
  }
  if (!haveComms) {
    return ((now / 350) & 1) != 0;
  }
  return true;
}

void nodeDiagBegin() {
#if SHOWDUINO_AUDIO_STATUS_LED_PIN >= 0
  pinMode(SHOWDUINO_AUDIO_STATUS_LED_PIN, OUTPUT);
  digitalWrite(SHOWDUINO_AUDIO_STATUS_LED_PIN, LOW);
#endif
  sMinHeap = ESP.getFreeHeap();
}

void nodeDiagMarkLoop(uint32_t elapsedUs) {
  sLoopUs = elapsedUs;
  if (elapsedUs > sLoopMaxUs) sLoopMaxUs = elapsedUs;
  const uint32_t heap = ESP.getFreeHeap();
  if (heap < sMinHeap) sMinHeap = heap;
}

void nodeDiagPrintBootBanner() {
  char mac[24];
  audioEspNowMacString(mac, sizeof(mac));
  const uint32_t flashMb = ESP.getFlashChipSize() / (1024UL * 1024UL);
  const uint32_t psram = ESP.getPsramSize();

  Serial.printf("[AUDIO NODE] ESP32 revision: %u (%s)\n",
                (unsigned)ESP.getChipRevision(), ESP.getChipModel());
  Serial.printf("[AUDIO NODE] Flash: %lu MB\n", (unsigned long)flashMb);
  Serial.printf("[AUDIO NODE] PSRAM: %s\n",
                psram ? "present" : "none (runtime)");
  Serial.printf("[AUDIO NODE] ES8388: %s addr=0x%02X\n",
                audioCodecReady() ? "ready" : "FAULT",
                (unsigned)audioCodecI2cAddress());
  Serial.printf("[AUDIO NODE] SD: %s\n", audioStorageLastError());
  Serial.printf("[AUDIO NODE] MAC: %s\n", mac);
  Serial.printf("[AUDIO NODE] Firmware: %s\n", SHOWDUINO_AUDIO_NODE_FW);

  Serial.println("================================================");
  Serial.println(" SHOWDUINO AUDIO NODE");
  Serial.println("================================================");
  Serial.printf("Name: %s\n", SHOWDUINO_AUDIO_NODE_NAME);
  Serial.printf("Board: %s\n", SHOWDUINO_AUDIO_NODE_BOARD);
  Serial.printf("Protocol: %s  Caps: %s\n", SHOWDUINO_AUDIO_PROTOCOL, SHOWDUINO_AUDIO_CAPS);
  Serial.printf("Output: %s  HP detect: %s\n",
                audioCodecOutputName(),
                audioCodecHpInserted() ? "JACK" : "OPEN");
  Serial.printf("ESP-NOW: %s ch=%u\n",
                audioEspNowReady() ? "ready" : "FAULT",
                (unsigned)SHOWDUINO_ESPNOW_CHANNEL);
  Serial.printf("State: %s\n", audioNodeStateName());
  if (audioStorageConfigFault()) Serial.println("CONFIG_FAULT — defaults in use");
  Serial.println("Waiting for Showduino.");
}

void nodeDiagPrintHelp() {
  Serial.println("[CONSOLE] Audio Node commissioning commands:");
  Serial.println("  HELP");
  Serial.println("  STATUS");
  Serial.println("  MAC");
  Serial.println("  STORAGE:STATUS");
  Serial.println("  ASSET:LIST");
  Serial.println("  AUDIO:STATUS");
  Serial.println("  AUDIO:TEST");
  Serial.println("  AUDIO:PLAY:<path>");
  Serial.println("  AUDIO:LOOP:<path>");
  Serial.println("  AUDIO:STOP");
  Serial.println("  AUDIO:PAUSE");
  Serial.println("  AUDIO:RESUME");
  Serial.println("  AUDIO:VOLUME:<0-100>");
  Serial.println("  KEYS:STATUS");
  Serial.println("  LED:TEST");
  Serial.println("  CODEC:STATUS");
  Serial.println("  RUN:TEST");
  Serial.println("  SOUND:STATUS | ENABLE | DISABLE | CALIBRATE | LEVEL");
  Serial.println("  SOUND:TRIGGER:TEST | CONFIG | MONITOR | MONITOR:STOP");
  Serial.println("  SOUND:RECORD:TEST | SOUND:LOCAL_TEST_TRIGGER");
  Serial.println("RUN:TEST is silent. Audible speaker test is AUDIO:TEST at current volume.");
}

void nodeDiagPrintStatus() {
  char mac[24];
  audioEspNowMacString(mac, sizeof(mac));
  Serial.printf("STATE %s\n", audioNodeStateName());
  Serial.printf("FAULT %s\n", showduino_audio_fail_name(audioNodeStateFault()));
  Serial.printf("CONFIG %s\n", audioStorageConfigFault() ? "CONFIG_FAULT" : "OK");
  Serial.printf("SHOW_CONTROLLED %s\n", audioNodeStateShowControlled() ? "YES" : "NO");
  Serial.printf("AUTHORITY %s\n",
                audioNodeStateAuthorityFresh(audioStorageConfig().commsTimeoutMs)
                    ? "FRESH" : "STALE");
  Serial.printf("MAC %s\n", mac);
  Serial.printf("VOLUME %u duck=%s\n",
                (unsigned)audioCommandVolume(),
                audioCommandDucking() ? "YES" : "NO");
  Serial.printf("OUTPUT %s\n", audioCodecOutputName());
  Serial.printf("FILE %s\n", audioPlaybackRel()[0] ? audioPlaybackRel() : "-");
  Serial.printf("CAPS %s\n", SHOWDUINO_AUDIO_CAPS);
}

void nodeDiagPrintAudio() {
  Serial.printf("CODEC %s addr=0x%02X out=%s\n",
                audioCodecReady() ? "OK" : "FAULT",
                (unsigned)audioCodecI2cAddress(),
                audioCodecOutputName());
  Serial.printf("PLAYING %s paused=%s loop=%s\n",
                audioPlaybackActive() ? "YES" : "NO",
                audioPlaybackPaused() ? "YES" : "NO",
                audioPlaybackLooping() ? "YES" : "NO");
  Serial.printf("FILE %s\n", audioPlaybackPath()[0] ? audioPlaybackPath() : "-");
  Serial.printf("POS %lu / %lu\n",
                (unsigned long)audioPlaybackPosition(),
                (unsigned long)audioPlaybackDuration());
  Serial.printf("PLAYED %lu completed=%lu failed=%lu\n",
                (unsigned long)audioPlaybackFilesPlayed(),
                (unsigned long)audioPlaybackCompleted(),
                (unsigned long)audioPlaybackFailed());
  Serial.printf("BYTES %lu underruns=%lu start_ms=%lu\n",
                (unsigned long)audioPlaybackBytesRead(),
                (unsigned long)audioPlaybackUnderruns(),
                (unsigned long)audioCommandLastStartLatencyMs());
  Serial.printf("ERR %s\n", audioPlaybackLastError());
}

void nodeDiagPrintCodec() {
  Serial.printf("ES8388 %s addr=0x%02X err=%s\n",
                audioCodecReady() ? "OK" : "FAULT",
                (unsigned)audioCodecI2cAddress(),
                audioCodecLastError()[0] ? audioCodecLastError() : "-");
  Serial.printf("OUTPUT %s HP=%s PA_GPIO=%d\n",
                audioCodecOutputName(),
                audioCodecHpInserted() ? "JACK" : "OPEN",
                SHOWDUINO_AUDIO_PA_PIN);
}

void nodeDiagPrintEspNow() {
  Serial.printf("ESPNOW %s comms=%s rx=%lu tx=%lu rej=%lu\n",
                audioEspNowReady() ? "OK" : "FAULT",
                audioEspNowHaveComms() ? "YES" : "NO",
                (unsigned long)audioEspNowRxCount(),
                (unsigned long)audioEspNowTxCount(),
                (unsigned long)audioEspNowRejected());
}

void nodeDiagPrintSd() {
  Serial.printf("SD %s writable=%s present=%s\n",
                audioStorageReady() ? "OK" : "OFFLINE",
                audioStorageWritable() ? "YES" : "NO",
                audioStorageCardPresent() ? "YES" : "NO");
  Serial.printf("STATE %s ERR %s\n", audioStorageStateName(), audioStorageLastError());
}

void nodeDiagPrintAssets() {
  char names[SHOWDUINO_AUDIO_INV_MAX][40];
  const uint16_t n = audioStorageInventory(names, SHOWDUINO_AUDIO_INV_MAX);
  Serial.printf("ASSETS %u (page size %u)\n",
                (unsigned)n, (unsigned)SHOWDUINO_AUDIO_INV_PER_PAGE);
  for (uint16_t i = 0; i < n; i++) Serial.printf("  %s\n", names[i]);
}

void nodeDiagPrintMetrics() {
  Serial.printf("HEAP free=%lu min=%lu\n",
                (unsigned long)ESP.getFreeHeap(), (unsigned long)sMinHeap);
  Serial.printf("PSRAM %lu\n", (unsigned long)ESP.getPsramSize());
  Serial.printf("LOOP us=%lu max=%lu\n",
                (unsigned long)sLoopUs, (unsigned long)sLoopMaxUs);
  Serial.printf("SD_ERR %s underruns=%lu start_ms=%lu\n",
                audioStorageLastError(),
                (unsigned long)audioPlaybackUnderruns(),
                (unsigned long)audioCommandLastStartLatencyMs());
  nodeDiagPrintEspNow();
}

void nodeDiagPrintRunTest() {
  Serial.println("[TEST] Audio Node commissioning (silent)");
  Serial.printf("  ESP32 rev %u flash=%luMB psram=%s\n",
                (unsigned)ESP.getChipRevision(),
                (unsigned long)(ESP.getFlashChipSize() / (1024UL * 1024UL)),
                ESP.getPsramSize() ? "YES" : "NO");
  Serial.printf("  ES8388 %s I2C=0x%02X\n",
                audioCodecReady() ? "PASS" : "FAIL",
                (unsigned)audioCodecI2cAddress());
  Serial.printf("  SD %s\n", audioStorageReady() ? "PASS" : "FAIL");
  Serial.printf("  dirs /showduino/audio/{ambience,effects,dialogue,music,stingers,test}\n");
  localButtonsPrintStatus();
  Serial.printf("  ESP-NOW %s comms=%s\n",
                audioEspNowReady() ? "PASS" : "FAIL",
                audioEspNowHaveComms() ? "YES" : "NO");
  Serial.println("  Speaker playback is NOT part of RUN:TEST. Use AUDIO:TEST.");
  nodeDiagLedTest();
}

void nodeDiagLedTest() {
  sLedTestUntil = millis() + 1500;
}

void nodeDiagServiceLed() {
#if SHOWDUINO_AUDIO_STATUS_LED_PIN < 0
  return;
#else
  const uint32_t now = millis();
  bool on;
  if ((int32_t)(sLedTestUntil - now) > 0) {
    on = ((now / 80) & 1) != 0;
  } else {
    on = ledOnForState(now);
  }
  digitalWrite(SHOWDUINO_AUDIO_STATUS_LED_PIN, on ? HIGH : LOW);
#endif
}

bool nodeDiagHandleLine(const char *line) {
  if (!line || !line[0]) return false;
  if (!strcmp(line, "HELP")) {
    nodeDiagPrintHelp();
    return true;
  }
  if (!strcmp(line, "STATUS")) {
    nodeDiagPrintStatus();
    nodeDiagPrintMetrics();
    return true;
  }
  if (!strcmp(line, "MAC")) {
    char mac[24];
    audioEspNowMacString(mac, sizeof(mac));
    Serial.println(mac);
    return true;
  }
  if (!strcmp(line, "STORAGE:STATUS") || !strcmp(line, "SD:STATUS")) {
    nodeDiagPrintSd();
    return true;
  }
  if (!strcmp(line, "ASSET:LIST")) {
    nodeDiagPrintAssets();
    return true;
  }
  if (!strcmp(line, "AUDIO:STATUS")) {
    nodeDiagPrintAudio();
    return true;
  }
  if (!strcmp(line, "KEYS:STATUS") || !strcmp(line, "BUTTONS:STATUS")) {
    localButtonsPrintStatus();
    return true;
  }
  if (!strcmp(line, "LED:TEST")) {
    nodeDiagLedTest();
    Serial.println("[LED] test pattern 1.5 s");
    return true;
  }
  if (!strcmp(line, "CODEC:STATUS")) {
    nodeDiagPrintCodec();
    return true;
  }
  if (!strcmp(line, "ESPNOW:STATUS")) {
    nodeDiagPrintEspNow();
    return true;
  }
  if (!strcmp(line, "RUN:TEST")) {
    nodeDiagPrintRunTest();
    return true;
  }
  if (!strncmp(line, "SOUND:", 6)) return false;
  return false;
}
