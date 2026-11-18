#include <cassert>
#include <initializer_list>
#include "../../firmware/stage-engine-p4/ShowduinoStageEngineP4/src/SystemSoundVolume.h"
int main() {
  uint8_t value=42;
  for(unsigned n=0;n<=100;++n) {
    char text[4] = { char('0'+n/100), char('0'+(n/10)%10), char('0'+n%10), 0 };
    assert(parseSystemSoundVolume(text,&value) && value==n);
    assert(systemSoundOutputVolume(value,false)==n);
    assert(systemSoundOutputVolume(value,true)==80);
  }
  for(const char *bad : {"", "-1", "101", "999999999999999", "50x", " 50", "50 ", "+5"}) {
    value=42; assert(!parseSystemSoundVolume(bad,&value)); assert(value==42);
  }
  assert(!parseSystemSoundVolume(nullptr,&value));
  assert(!parseSystemSoundVolume("50",nullptr));
  assert(systemSoundOutputVolume(255,false)==80);
}
