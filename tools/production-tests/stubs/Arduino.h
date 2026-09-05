#ifndef SHOWDUINO_HOST_TEST_ARDUINO_H
#define SHOWDUINO_HOST_TEST_ARDUINO_H

#include <cstdint>
#include <cstdio>

extern uint32_t gShowduinoTestMillis;

inline uint32_t millis() { return gShowduinoTestMillis; }
inline void yield() {}

struct ShowduinoTestSerial {
  template <typename... Args> void printf(const char *, Args...) {}
  template <typename T> void print(const T &) {}
  template <typename T> void println(const T &) {}
  void println() {}
};

extern ShowduinoTestSerial Serial;

#endif
