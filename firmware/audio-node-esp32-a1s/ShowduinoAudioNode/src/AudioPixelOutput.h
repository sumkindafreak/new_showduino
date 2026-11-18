#pragma once
#include <Arduino.h>
#include "esp32-hal-rmt.h"

// Independent RMT channels for status and show output; failures are observable.
class AudioPixelOutput {
public:
  bool begin(int pin, size_t bytes);
  bool write(const uint8_t *bytes, size_t length);
  void end();
private:
  int pin_ = -1;
  rmt_data_t *symbols_ = nullptr;
  size_t bytes_ = 0;
  uint32_t lastErrorMs_ = 0;
  bool errorLogged_ = false;
};
