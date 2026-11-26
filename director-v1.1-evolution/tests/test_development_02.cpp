#include "../previews/DirectorV11Preview.h"
#include "../components/DirectorV11Health.h"
#include "../ui/DirectorV11DesignTokens.h"
#include <algorithm>
#include <cassert>
#include <cstdint>

using namespace ShowduinoV11;

int main() {
  Preview p;
  PreviewSafety safe{false,true,true,true,true,true};
  PreviewSafety unsafe;
  assert(!p.tick(0,unsafe));
  for (unsigned effect=0; effect<8; ++effect) {
    p.select(static_cast<PreviewEffect>(effect));
    assert(p.tick(100,safe));
    assert(!p.tick(199,safe));
    assert(p.tick(200,safe));
    assert(p.tick(201,unsafe));
    assert(!p.running());
    assert(std::all_of(p.frame().begin(),p.frame().end(),[](uint16_t x){return x==0;}));
    assert(!p.tick(202,unsafe));
  }
  p.select(PreviewEffect::Rainbow);
  assert(p.tick(UINT32_MAX-50,safe));
  assert(!p.tick(20,safe));
  assert(p.tick(50,safe));
  for (unsigned i=0;i<6;++i) {
    PreviewSafety blocked=safe;
    switch(i) {
      case 0: blocked.emergency=true; break;
      case 1: blocked.p4StateFresh=false; break;
      case 2: blocked.showIdle=false; break;
      case 3: blocked.pageVisible=false; break;
      case 4: blocked.memoryOk=false; break;
      case 5: blocked.uiLoadOk=false; break;
    }
    p.select(PreviewEffect::Fire);
    assert(p.tick(1,safe));
    assert(p.tick(2,blocked));
    assert(!p.running());
  }
  HealthEvidence e;
  assert(classifyHealth(e)==Health::LinkLost);
  e.commsOnline=e.p4Online=e.p4StateFresh=e.p4Ready=true;
  assert(classifyHealth(e)==Health::Unknown);
  e.emergencyKnown=e.memoryKnown=e.memoryOk=e.storageKnown=e.storageOk=true;
  assert(classifyHealth(e)==Health::Ready);
  e.storageOk=false; assert(classifyHealth(e)==Health::Degraded);
  e.fault=true; assert(classifyHealth(e)==Health::Fault);
  e.p4StateFresh=false; assert(classifyHealth(e)==Health::Stale);
  e.emergency=true; assert(classifyHealth(e)==Health::Emergency);
  ChangeTracker<Health> tracker;
  assert(tracker.update(Health::Ready)); assert(!tracker.update(Health::Ready));
  tracker.invalidate(); assert(tracker.update(Health::Ready));
  Freshness f; assert(!f.fresh(0,100));
  f.observe(UINT32_MAX-20); assert(f.fresh(20,100)); assert(!f.fresh(100,100));
  f.invalidate(); assert(!f.fresh(20,100));
}
