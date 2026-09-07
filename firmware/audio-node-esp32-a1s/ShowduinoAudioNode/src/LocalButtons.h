#ifndef SHOWDUINO_AUDIO_LOCAL_BUTTONS_H
#define SHOWDUINO_AUDIO_LOCAL_BUTTONS_H

#include <Arduino.h>
#include "../../../protocol/showduino_audio_node.h"

struct LocalButtonEvent {
  ShowduinoAudioButton btn;
  bool longPress;
};

void localButtonsBegin();
LocalButtonEvent localButtonsPoll();
void localButtonsPrintStatus();

#endif
