#pragma once
#include <stdint.h>
class DirectorRuntimeFreshness {
public:
  void received(uint32_t now) { seen_ = true; receivedMs_ = now; }
  bool fresh(uint32_t now) const { return seen_ && uint32_t(now-receivedMs_) < 2500; }
  bool queryDue(uint32_t now, bool linked) {
    if (!linked || fresh(now)) return false;
    if (queried_ && uint32_t(now-queryMs_) < 2000) return false;
    queried_ = true; queryMs_ = now; return true;
  }
private:
  bool seen_ = false, queried_ = false;
  uint32_t receivedMs_ = 0, queryMs_ = 0;
};
