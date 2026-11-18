#include "AudioPixelOutput.h"
#include <stdlib.h>

bool AudioPixelOutput::begin(int pin, size_t bytes) {
  end();
  if (!bytes || bytes > 512U * 3U) return false;
  symbols_ = (rmt_data_t *)calloc(bytes * 8U + 1U, sizeof(rmt_data_t));
  if (!symbols_ || !rmtInit(pin, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, 10000000)) {
    Serial.printf("[PIXEL][ERROR] GPIO%d RMT allocation/init failed\n", pin);
    free(symbols_); symbols_ = nullptr;
    return false;
  }
  pin_ = pin;
  bytes_ = bytes;
  Serial.printf("[PIXEL] GPIO%d RMT output ready (%u pixels)\n", pin_, (unsigned)(bytes_ / 3U));
  return true;
}

bool AudioPixelOutput::write(const uint8_t *bytes, size_t length) {
  if (pin_ < 0 || !symbols_ || !bytes || length != bytes_) return false;
  size_t n = 0;
  for (size_t i = 0; i < length; ++i) {
    for (uint8_t bit = 0x80; bit; bit >>= 1) {
      rmt_data_t &s = symbols_[n++];
      const bool one = (bytes[i] & bit) != 0;
      s.level0 = 1; s.duration0 = one ? 8 : 4;
      s.level1 = 0; s.duration1 = one ? 4 : 8;
    }
  }
  rmt_data_t &reset = symbols_[n++];
  reset.level0 = 0; reset.duration0 = 600;
  reset.level1 = 0; reset.duration1 = 600;
  const bool ok = rmtWrite(pin_, symbols_, n, 80);
  if (!ok && (!errorLogged_ || millis() - lastErrorMs_ >= 1000U)) {
    Serial.printf("[PIXEL][ERROR] GPIO%d RMT write failed/timed out\n", pin_);
    lastErrorMs_ = millis(); errorLogged_ = true;
  }
  return ok;
}

void AudioPixelOutput::end() {
  if (pin_ >= 0) rmtDeinit(pin_);
  pin_ = -1; bytes_ = 0;
  free(symbols_); symbols_ = nullptr;
  errorLogged_ = false;
}
