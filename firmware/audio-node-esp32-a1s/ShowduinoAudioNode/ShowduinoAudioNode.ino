/*
  Showduino Audio Node — Ai-Thinker ESP32-A1S Audio Kit V2.2 A161 / ES8388

  Programme / attraction audio only.
  P4 onboard ES8311 remains Showduino system / safety audio.
*/

#include <Arduino.h>
#include "BoardConfig.h"
#include "src/AudioCodec.h"
#include "src/AudioStorage.h"
#include "src/AudioPlayback.h"
#include "src/AudioNodeState.h"
#include "src/AudioCommand.h"
#include "src/EspNowNodeTransport.h"
#include "src/LocalButtons.h"
#include "src/NodeDiagnostics.h"
#include "src/input/AudioInput.h"
#include "../../../protocol/showduino_audio_node.h"

static String sUsbLine;

static void onEspNowCommand(const char *command, uint32_t sequence) {
  SD_LOGT("ESPNOW", "RX seq=%lu cmd=%s", (unsigned long)sequence,
          command ? command : "");
  audioCommandApply(command, sequence, SHOWDUINO_CMD_ORIGIN_SHOW);
}

static void handleUsbLine(const String &line) {
  if (!line.length()) return;
  if (nodeDiagHandleLine(line.c_str())) return;
  audioCommandApply(line.c_str(), 0, SHOWDUINO_CMD_ORIGIN_LOCAL);
}

static void pollUsb() {
  while (Serial.available() > 0) {
    const char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      sUsbLine.trim();
      if (sUsbLine.length()) handleUsbLine(sUsbLine);
      sUsbLine = "";
    } else if (sUsbLine.length() < 120) {
      sUsbLine += c;
    }
  }
}

static void pollButtons() {
  const LocalButtonEvent ev = localButtonsPoll();
  if (ev.btn == SHOWDUINO_AUDIO_BTN_NONE) return;

  if (ev.btn == SHOWDUINO_AUDIO_BTN_PLAY) {
    Serial.printf("[BUTTON] KEY1 %s\n", ev.longPress ? "LONG" : "SHORT");
  } else if (ev.btn == SHOWDUINO_AUDIO_BTN_VOL_UP) {
    Serial.println("[BUTTON] KEY4 VOL+");
  } else if (ev.btn == SHOWDUINO_AUDIO_BTN_VOL_DOWN) {
    Serial.println("[BUTTON] KEY3 VOL-");
  } else if (ev.btn == SHOWDUINO_AUDIO_BTN_PREV) {
    Serial.println("[BUTTON] KEY5 PREV");
  } else if (ev.btn == SHOWDUINO_AUDIO_BTN_NEXT) {
    Serial.println("[BUTTON] KEY6 NEXT");
  }

  if (!showduino_audio_button_allowed(audioNodeState(),
                                      audioNodeStateShowControlled() ? 1 : 0,
                                      ev.btn)) {
    return;
  }

  if (ev.btn == SHOWDUINO_AUDIO_BTN_PLAY) {
    if (ev.longPress) {
      if (audioPlaybackActive() || audioPlaybackPaused()) audioCommandLocalStop();
      else nodeDiagPrintStatus();
    } else {
      audioCommandLocalTestToggle();
    }
  } else if (ev.btn == SHOWDUINO_AUDIO_BTN_VOL_UP) {
    audioCommandNudgeVolume(5);
  } else if (ev.btn == SHOWDUINO_AUDIO_BTN_VOL_DOWN) {
    audioCommandNudgeVolume(-5);
  } else if (ev.btn == SHOWDUINO_AUDIO_BTN_PREV) {
    audioCommandLocalPrev();
  } else if (ev.btn == SHOWDUINO_AUDIO_BTN_NEXT) {
    audioCommandLocalNext();
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  nodeDiagBegin();
  audioNodeStateBegin(SHOWDUINO_AUDIO_ST_BOOTING);

  const bool codecOk = audioCodecBegin();
  const bool sdOk = audioStorageBegin();
  audioPlaybackBegin();
  localButtonsBegin();
  audioEspNowBegin();
  audioEspNowSetHandler(onEspNowCommand);

  if (!codecOk) {
    audioNodeStateSetFault(SHOWDUINO_AUDIO_FAIL_CODEC);
  } else if (!sdOk) {
    audioNodeStateSetFault(SHOWDUINO_AUDIO_FAIL_NO_STORAGE);
  } else {
    audioNodeStateSet(SHOWDUINO_AUDIO_ST_IDLE);
  }

  audioCommandBegin(audioStorageConfig().volume);
  audioInputBegin();
  nodeDiagPrintBootBanner();
  audioCommandAnnounce();
}

void loop() {
  const uint32_t t0 = micros();
  pollUsb();
  pollButtons();
  audioStorageLoop();
  audioInputService();
  audioCommandService();
  nodeDiagServiceLed();
  nodeDiagMarkLoop(micros() - t0);
}
