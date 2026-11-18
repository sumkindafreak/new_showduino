#include "support.h"
#ifndef AUDIO_ENGINE_SOURCE
#define AUDIO_ENGINE_SOURCE "../../firmware/audio-node-esp32-a1s/ShowduinoAudioNode/src/AudioPixelEngine.cpp"
#endif
#include AUDIO_ENGINE_SOURCE
static uint16_t savedCount=0;
static uint8_t savedBrightness=255;
uint16_t nodeConfigGetU16(const char*,uint16_t){return savedCount;}
uint8_t nodeConfigGetU8(const char*,uint8_t){return savedBrightness;}
void nodeConfigSetU16(const char* key,uint16_t v){savedCount=v;printf("CONFIG16 %s %u\n",key,unsigned(v));}
void nodeConfigSetU8(const char* key,uint8_t v){savedBrightness=v;printf("CONFIG8 %s %u\n",key,unsigned(v));}
static unsigned samples=0;static uint32_t digest=2166136261U;
static void capture(){
  std::vector<uint8_t> expected;
  for(unsigned i=0;i<sCount;++i){
#if SHOWDUINO_AUDIO_FASTLED_RGB_FRAME
    expected.insert(expected.end(),{sFrame[i].g,sFrame[i].r,sFrame[i].b});
#else
    expected.push_back(sFrame[i*3+1]);expected.push_back(sFrame[i*3]);expected.push_back(sFrame[i*3+2]);
#endif
  }
  expectBytes(22,expected);for(auto b:expected)digest=(digest^b)*16777619U;++samples;
  printf("STATE %u %u %u %u %u %u %u\n",testNow,sReady,sEmergency,sTestActive,sLocateActive,unsigned(sCount),unsigned(sGlobalBrightness));
}
static std::string command(const char* cmd){char reply[180];assert(audioPixelEngineHandleCommand(cmd,reply,sizeof(reply)));printf("COMMAND %s -> %s\n",cmd,reply);return reply;}
static void step(unsigned n){for(unsigned i=0;i<n;++i){testNow+=25;audioPixelEngineService();capture();}}
static void fillWhite(){auto bytes=decode(channels[22].writes.back());assert(std::all_of(bytes.begin(),bytes.end(),[](uint8_t b){return b==255;}));}
static void fillBlack(){auto bytes=decode(channels[22].writes.back());assert(std::all_of(bytes.begin(),bytes.end(),[](uint8_t b){return b==0;}));}
int main(int argc,char** argv){
  (void)argc;(void)argv;
#if SHOWDUINO_AUDIO_FASTLED_RGB_FRAME
  if(argc>1&&std::strcmp(argv[1],"--render-oom")==0){
    audioPixelEngineSetConfiguredCount(10);failNextRenderAllocation=true;
    assert(!audioPixelEngineBegin()&&!audioPixelEngineReady()&&!channels[22].active&&outputAllocations.empty());
    assert(audioPixelEngineBegin()&&audioPixelEngineReady());
    audioPixelEngineRelease();assert(outputAllocations.empty());
    puts("PASS real FastLED frame allocation failure/retry/cleanup");return 0;
  }
#endif
  for(uint16_t stored:{uint16_t(0),uint16_t(1),uint16_t(9),uint16_t(513),uint16_t(30),uint16_t(512)}){
    savedCount=stored;audioPixelEngineApplyPersisted();
    const uint16_t want=stored==30||stored==512?stored:10;
    assert(audioPixelEngineReady()&&audioPixelEngineConfiguredCount()==want&&audioPixelEngineCount()==want);
    audioPixelEngineBlackout();fillBlack();capture();audioPixelEngineRelease();assert(outputAllocations.empty());
  }
  audioPixelEngineSetConfiguredCount(10);
  failNextOutputAllocation=true;assert(!audioPixelEngineBegin()&&!audioPixelEngineReady());assert(outputAllocations.empty());
  channels[22].failInit=true;assert(!audioPixelEngineBegin());channels[22].failInit=false;
  channels[22].failWrite=true;assert(!audioPixelEngineBegin()&&!audioPixelEngineReady());channels[22].failWrite=false;
  assert(outputAllocations.empty()&&audioPixelEngineBegin());
  assert(command("PIXEL:SOLID:18:52:86")=="PIXEL:SOLID:OK");step(1);
  std::vector<uint8_t> mixed;for(unsigned i=0;i<10;++i)mixed.insert(mixed.end(),{52,18,86});expectBytes(22,mixed);
  for(unsigned brightness:{0U,1U,127U,255U}){
    char line[64];snprintf(line,sizeof(line),"PIXEL:BRIGHTNESS:%u",brightness);command(line);step(2);
    auto bytes=decode(channels[22].writes.back());
    assert(bytes[0]==52*brightness/255&&bytes[1]==18*brightness/255&&bytes[2]==86*brightness/255);
  }
  // Actual engine frame -> actual vendor GRB buffer -> actual encoder for every effect ID.
  for(unsigned fx=0;fx<static_cast<unsigned>(ShowduinoPixelFx::Count);++fx){
    char line[80];snprintf(line,sizeof(line),"PIXEL:SEGMENT:0:FX:%s",showduinoPixelFxName(static_cast<ShowduinoPixelFx>(fx)));
    assert(command(line)=="PIXEL:SEGMENT:FX:OK");command("PIXEL:SEGMENT:0:START");step(12);
  }
  command("PIXEL:BRIGHTNESS:0");audioPixelEngineStartLocate();
  audioPixelEngineOnEmergency(true);fillWhite();assert(!audioPixelEngineLocateActive());
  for(const char* c:{"PIXEL:OFF","PIXEL:TEST","PIXEL:LOCATE","PIXEL:COUNT:30","PIXEL:INIT","PIXEL:BRIGHTNESS:0","PIXEL:SOLID:0:0:0"}){
    assert(command(c)=="PIXEL:REJECTED:EMERGENCY_ACTIVE");fillWhite();
  }
  audioPixelEngineBlackout();fillWhite();audioPixelEngineStartLocate();assert(!audioPixelEngineLocateActive());
  testNow+=250;audioPixelEngineService();fillWhite();capture();
  channels[22].failWrite=true;testNow+=250;audioPixelEngineService();assert(!channels[22].writes.back().success);
  channels[22].failWrite=false;testNow+=250;audioPixelEngineService();fillWhite();
  audioPixelEngineOnEmergency(false);fillBlack();
  // The existing "ActiveSegments" status getter counts configured ranges,
  // not running effects. Check actual active flags for the no-resume gate.
  assert(audioPixelEngineActiveSegments()==1);
  for(const auto& seg:sSegments)assert(!seg.active);
  step(5);fillBlack();
  for(unsigned count:{30U,512U,10U}){char line[40];snprintf(line,sizeof(line),"PIXEL:COUNT:%u",count);command(line);command("PIXEL:INIT");assert(audioPixelEngineCount()==count);fillBlack();capture();}
  audioPixelEngineRelease();assert(!channels[22].active&&outputAllocations.empty());
  printf("PASS actual engine + vendor buffer + encoder: %u frame samples, digest=%08x, defaults/counts/effects/brightness/Emergency/failures/rebind\n",samples,digest);
  dumpAudioTrace();
}
