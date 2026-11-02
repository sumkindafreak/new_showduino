#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
struct HostSerial {
  template<typename... T> void printf(const char *, T...) {}
  template<typename T> void println(const T &) {}
};
inline HostSerial Serial;
