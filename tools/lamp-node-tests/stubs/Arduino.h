#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdio>
inline uint32_t testNow = 0;
inline uint32_t millis() { return testNow; }
constexpr int OUTPUT=1, LOW=0;
inline void pinMode(int,int) {}
inline void digitalWrite(int,int) {}
struct TestSerial {
  template<class... T> void printf(const char*,T...) {}
  void println(const char*) {}
};
inline TestSerial Serial;
