#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdio>
inline uint32_t testNow=1;
inline uint32_t millis(){return testNow;}
struct TestSerial {
  template<class... T> void printf(const char*,T...){}
  void println(const char*){}
};
inline TestSerial Serial;
