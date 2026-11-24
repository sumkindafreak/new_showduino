#include "../diagnostics/DirectorV11Resources.h"
#include <cassert>
using namespace ShowduinoV11;
int main() {
  ResourceMonitor m;
  assert(!m.snapshot().valid);
  ResourceSample s{true, 100, 60, 200};
  ResourceBudget b{90, 50, 150, true};
  assert(resourceBudgetOk(s,b));
  b.configured=false; assert(!resourceBudgetOk(s,b)); b.configured=true;
  m.observeMemory(s); m.observeMemory({true, 80, 30, 190});
  m.observeMemory({true, 90, 100, 10}); // inconsistent sample ignored
  m.observeMemory({false, 0, 0, 0});
  m.observeLoop(200); m.observeLoop(100);
  m.observeTransition(300); m.observeTransition(250);
  auto r=m.snapshot();
  assert(r.valid && r.samples==2);
  assert(r.minimumInternal==80 && r.minimumLargest==30 && r.minimumPsram==190);
  assert(r.maximumLoopUs==200 && r.maximumTransitionUs==300);
  assert(!resourceBudgetOk({true,80,30,190},b));
  m.clear(); assert(!m.snapshot().valid && m.snapshot().maximumLoopUs==0);
}
