#include <cassert>
#include "../../firmware/director-esp32-8048s050/ShowduinoDirector8048S050/DirectorRuntimeFreshness.h"
int main() {
  DirectorRuntimeFreshness f;
  assert(!f.fresh(0)); assert(!f.queryDue(0,false)); assert(f.queryDue(0,true));
  // Lost initial response, despite healthy heartbeats: bounded retries.
  assert(!f.queryDue(1999,true)); assert(f.queryDue(2000,true));
  f.received(2100); assert(f.fresh(4599)); assert(!f.queryDue(4599,true));
  assert(!f.fresh(4600)); assert(f.queryDue(4600,true));
  // Lost query and response never trigger commands other than another query.
  assert(!f.queryDue(5000,true)); assert(f.queryDue(6600,true));
  f.received(6700); assert(f.fresh(6700)); assert(!f.queryDue(6700,true));
  assert(!f.queryDue(20000,false)); assert(f.queryDue(20001,true));
  f.received(UINT32_MAX-100); assert(f.fresh(100)); assert(!f.fresh(2400));
  DirectorRuntimeFreshness wrap;
  assert(wrap.queryDue(UINT32_MAX-100,true)); assert(!wrap.queryDue(100,true));
  assert(wrap.queryDue(1900,true));
  for(uint32_t now=0;now<100000;now+=1000) { f.received(now); assert(!f.queryDue(now,true)); }
}
