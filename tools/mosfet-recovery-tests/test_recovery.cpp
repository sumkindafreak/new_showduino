#define SHOWDUINO_LOG_H
#include <cassert>
#include <Preferences.h>
#include <thread>
#include <atomic>
#include <chrono>
#ifndef MOSFET_OUTPUT_SOURCE
#define MOSFET_CANDIDATE 1
#define MOSFET_OUTPUT_SOURCE "../../firmware/mosfet-node-esp32/ShowduinoMosfetNode/src/MosfetOutputEngine.cpp"
#endif
#ifndef MOSFET_STATE_SOURCE
#define MOSFET_STATE_SOURCE "../../firmware/mosfet-node-esp32/ShowduinoMosfetNode/src/MosfetNodeState.cpp"
#endif
#include MOSFET_OUTPUT_SOURCE
#include MOSFET_STATE_SOURCE
void mosfetIdentifierPixelsAllOff(){}
static void boot(){mosfetOutputEngineBegin();mosfetNodeStateBegin();}
static uint8_t level(unsigned i){
 uint8_t out[4];mosfetOutputEngineLevels(out);
 const unsigned pins[4]={16,17,26,27};
 assert(duty[pins[i]]==(unsigned(out[i])*255U)/100U);
 return out[i];
}
static void tick(uint32_t delta){testNow+=delta;mosfetOutputEngineLoop();mosfetNodeStateLoop();}
static void grant(){mosfetNodeStateOnGrant(testNow);}
int main(int argc,char**){
 flashRecords.clear();boot();grant();assert(mosfetOutputEngineSetLevel(1,37));
 tick(8001);assert(!mosfetNodeStateOwned());assert(level(0)==37); // Fails on original authority-loss all-off.
 if(argc>1)return 0;
 tick(10000);assert(level(0)==37);grant();assert(level(0)==37);
 assert(mosfetOutputEngineSetLevel(2,64));boot();assert(level(0)==0&&level(1)==0);grant();assert(level(0)==37&&level(1)==64);
 // A level explicitly changed while waiting overrides its saved boot value.
 boot();assert(mosfetOutputEngineSetLevel(1,25));grant();assert(level(0)==25&&level(1)==64);
 // Ownership reacquisition does not zero a standalone hold.
 tick(8001);assert(mosfetOutputEngineSetLevel(1,44));grant();assert(level(0)==44);
 // Pulse and fade pause at the loss-detection instant, holding the reached level.
 assert(mosfetOutputEnginePulse(3,80,20000));assert(mosfetOutputEngineFade(4,100,20000));
 tick(8001);const auto pulse=level(2),fade=level(3);assert(pulse==80&&fade==40);
 tick(30000);assert(level(2)==pulse&&level(3)==fade);grant();
 tick(1000);assert(level(2)==80&&level(3)==45);grant();
 tick(2000);grant();tick(2000);grant();tick(2000);grant();tick(2000);grant();tick(2000);grant();tick(1000);
 assert(level(2)==0&&level(3)==100);
 boot();grant();assert(level(3)==100); // A completed fade becomes a saved steady value.
 // Timed effects are cancelled across a MOSFET reboot; steady channels persist.
 assert(mosfetOutputEnginePulse(3,90,20000));assert(mosfetOutputEngineFade(4,20,20000));
 boot();assert(level(0)==0&&level(1)==0&&level(2)==0&&level(3)==0);grant();
 assert(level(0)==44&&level(1)==64&&level(2)==0&&level(3)==0);
 // Stop, clear and fault invalidate both warm resume and persistent restore.
 assert(mosfetOutputEnginePulse(3,80,20000));tick(8001);
 mosfetNodeStateOnEmergencyStop();grant();for(unsigned i=0;i<4;++i)assert(level(i)==0);
 mosfetNodeStateOnEmergencyClear();grant();for(unsigned i=0;i<4;++i)assert(level(i)==0);
 boot();grant();for(unsigned i=0;i<4;++i)assert(level(i)==0);
 assert(mosfetOutputEngineSetLevel(1,55));mosfetNodeStateSetFault("TEST");boot();grant();assert(level(0)==0);
 // Explicit off while paused cancels that channel's future resume.
 assert(mosfetOutputEnginePulse(1,80,20000));tick(8001);assert(mosfetOutputEngineOff(1));grant();assert(level(0)==0);
 assert(mosfetOutputEngineSetLevel(2,52));tick(8001);mosfetOutputEngineAllOff("CMD");grant();assert(level(1)==0);boot();grant();assert(level(1)==0);
 // Invalid saved record is refused. Failed commits retry without per-tick writes.
 flashRecords["sdmosresume:steady"]={1,101,0,0,0,0};boot();grant();for(unsigned i=0;i<4;++i)assert(level(i)==0);
 failStoreWrite=true;assert(mosfetOutputEngineSetLevel(1,29));const auto writes=storeWrites;
 tick(100);assert(storeWrites==writes);failStoreWrite=false;tick(1000);assert(storeWrites==writes+1);
 boot();assert(level(0)==0);grant();assert(level(0)==29);
 const auto committed=storeWrites;assert(mosfetOutputEngineSetLevel(1,29));assert(storeWrites==committed);
 flashRecords["sdmosresume:steady"]={1,101,0,0,0,uint8_t(0xa5^1^101)};boot();grant();assert(level(0)==0);
 flashRecords["sdmosresume:steady"]={2,29,0,0,0,uint8_t(0xa5^2^29)};boot();grant();assert(level(0)==0);
 assert(mosfetOutputEngineSetLevel(1,55));failStoreWrite=true;mosfetNodeStateOnEmergencyStop();grant();assert(level(0)==0);
 mosfetNodeStateOnEmergencyClear();grant();assert(level(0)==0);failStoreWrite=false;tick(1000);boot();grant();assert(level(0)==0);
 // Remaining pulse time survives millis() rollover during a warm control outage.
 testNow=UINT32_MAX-10000U;grant();assert(mosfetOutputEnginePulse(1,70,20000));tick(8001);assert(level(0)==70);
 tick(30000);grant();for(unsigned i=0;i<6;++i){tick(2000);grant();}assert(level(0)==0);
 failStoreBegin=true;boot();grant();assert(level(0)==0);failStoreBegin=false;
 #ifdef MOSFET_CANDIDATE
 // Force a slow old commit to overlap a later Emergency from another task.
 boot();grant();
 lockContention=0;
 {std::lock_guard<std::mutex> lock(flashTestMutex);pauseFlashWrite=true;flashWriteEntered=false;releaseFlashWrite=false;}
 std::thread earlier([]{assert(mosfetOutputEngineSetLevel(1,73));});
 {std::unique_lock<std::mutex> lock(flashTestMutex);flashTestCv.wait(lock,[]{return flashWriteEntered;});}
 std::atomic<bool> stopRequested{false};
 std::thread later([&]{stopRequested=true;mosfetNodeStateOnEmergencyStop();});
 while(!stopRequested.load())std::this_thread::yield();
 const auto waitStart=std::chrono::steady_clock::now();
 while(!lockContention.load()&&std::chrono::steady_clock::now()-waitStart<std::chrono::seconds(2))std::this_thread::yield();
 assert(lockContention.load()>0); // The later task really contends with the older commit.
 {std::lock_guard<std::mutex> lock(flashTestMutex);releaseFlashWrite=true;flashTestCv.notify_all();}
 earlier.join();later.join();assert(level(0)==0);boot();grant();assert(level(0)==0);
 sEngineMutex=nullptr;failMutexCreate=true;boot();grant();assert(level(0)==0);assert(!mosfetOutputEngineSetLevel(1,80));failMutexCreate=false;
 #endif
 puts("PASS actual MOSFET recovery: hold/regrant, cold steady restore, warm pulse/fade pause, cold timed cancel, Emergency/fault/off, storage validation/retry");
 puts("PASS serialized output/persistence: delayed old write cannot overwrite later Emergency; lock allocation fails off");
}
