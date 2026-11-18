#include "support.h"
#ifndef AUDIO_STATUS_SOURCE
#define AUDIO_STATUS_SOURCE "../../firmware/audio-node-esp32-a1s/ShowduinoAudioNode/src/AudioStatusPixel.cpp"
#endif
#include AUDIO_STATUS_SOURCE
static ShowduinoAudioNodeState state=SHOWDUINO_AUDIO_ST_IDLE;
static ShowduinoNodeOwnerMode owner=SHOWDUINO_OWNER_STANDALONE;
static ShowduinoAudioFail fault=SHOWDUINO_AUDIO_FAIL_NONE;
static bool emergency=false,locating=false,recording=false;
ShowduinoAudioNodeState audioNodeState(){return state;}
ShowduinoNodeOwnerMode audioOwnerMode(){return owner;}
ShowduinoAudioFail audioNodeStateFault(){return fault;}
bool audioPixelEngineEmergency(){return emergency;}
bool audioPixelEngineLocateActive(){return locating;}
bool audioInputRecording(){return recording;}
bool audioNodeStateAuthorityFresh(uint32_t){return false;}
uint32_t audioNodeStateLastCommsMs(){return 0;}
static void service(){audioStatusPixelService();printf("CACHE %u %u %u %u\n",testNow,sColour,sLastWriteMs,sTesting);}
int main(){
  channels[5].failInit=true;audioStatusPixelBegin();assert(!sOutputReady&&!channels[5].active&&outputAllocations.empty());
  channels[5].failInit=false;audioStatusPixelBegin();assert(sOutputReady);expectBytes(5,{0,12,24});
  auto writes=channels[5].writes.size();testNow+=1;service();assert(channels[5].writes.size()==writes);
  state=SHOWDUINO_AUDIO_ST_PLAYING;channels[5].failWrite=true;
  const auto previousColour=sColour,previousTime=sLastWriteMs;
  for(unsigned i=0;i<3;++i){++testNow;service();assert(!channels[5].writes.back().success&&sColour==previousColour&&sLastWriteMs==previousTime);}
  channels[5].failWrite=false;++testNow;service();expectBytes(5,{40,0,0});
  writes=channels[5].writes.size();++testNow;service();assert(channels[5].writes.size()==writes);
  testNow+=250;service();assert(channels[5].writes.size()==writes+1);
  state=SHOWDUINO_AUDIO_ST_PAUSED;++testNow;service();expectBytes(5,{18,40,0});
  // Each Emergency source must outrank fault, locate, recording and a requested test.
  state=SHOWDUINO_AUDIO_ST_FAULT;locating=true;recording=true;
  for(unsigned source=0;source<3;++source){
    state=source==0?SHOWDUINO_AUDIO_ST_EMERGENCY:SHOWDUINO_AUDIO_ST_FAULT;
    owner=source==1?SHOWDUINO_OWNER_EMERGENCY:SHOWDUINO_OWNER_STANDALONE;
    emergency=source==2;audioStatusPixelTest();++testNow;service();expectBytes(5,{255,255,255});assert(!sTesting);
  }
  state=SHOWDUINO_AUDIO_ST_IDLE;owner=SHOWDUINO_OWNER_STANDALONE;emergency=locating=recording=false;
  ++testNow;service();expectBytes(5,{0,12,24});
  assert(channels[5].writes.back().symbols.size()==25&&!channels[22].active);
  sStatusOutput.end();assert(outputAllocations.empty());
  puts("PASS actual one-pixel status + vendor + encoder: init recovery, failed-write cache/retry, refresh, playback colours, three Emergency sources and priority");
  dumpAudioTrace();
}
