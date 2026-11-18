#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <fstream>
#include <string>
#include <vector>
#include "../../protocol/showduino_shdo.h"
#include "ShowRuntimeOwner.h"
uint32_t gShowduinoTestMillis=0;
ShowduinoTestSerial Serial;
static int fired=0;
static std::vector<std::string> firedCommands;
static char last[64]={};
static void dispatch(const char *cmd){firedCommands.emplace_back(cmd);++fired;snprintf(last,sizeof(last),"%s",cmd);}
int main(){
 uint32_t time=999,fingerprint=0,revision=0;
 assert(showduinoParseSceneStart("SHOW:STARTAT:74000:1234abcd:42",&time,&fingerprint,&revision)&&time==74000&&fingerprint==0x1234abcdu&&revision==42);
 for(auto bad:{"SHOW:STARTAT:","SHOW:STARTAT:-1","SHOW:STARTAT:1junk","SHOW:STARTAT:4294967296","SHOW:STARTAT: 1"})assert(!showduinoParseSceneStart(bad,&time,&fingerprint,&revision));
 assert(showduinoSceneLabel("INTERNAL:LOG:a:SCENE Dino"));
 assert(!showduinoSceneLabel("INTERNAL:LOG:a:Not a scene"));
 ShowRuntimeOwner owner;ShowEngineState legacy;owner.begin(nullptr);owner.bootToIdle();owner.setDispatch(dispatch);
 assert(!owner.handleStartScene(0,0,&legacy,owner.sceneFingerprint(),owner.rt.revision));
 assert(owner.handleTlBegin(false));
 owner.handleTlCue(0,"INTERNAL:LOG:first:SCENE Boot");
 owner.handleTlCue(0,"AUDIO:NODE:PLAY:boot.wav");
 owner.handleTlCue(1000,"INTERNAL:LOG:dino:SCENE Dino");
 owner.handleTlCue(1000,"AMBIENCE:LOOP:/showduino/audio/ambience/whales.wav");
 owner.handleTlCue(1000,"AUDIO:NODE:PLAY:dinosaur.wav");
 owner.handleTlCue(2000,"INTERNAL:LOG:end:Complete");
 assert(owner.handleTlEnd(0,&legacy,"Recovery",false));
 uint32_t expected=2166136261u;
 for(unsigned i=0;i<owner.timeline.cueTotal();++i){const auto *c=owner.timeline.cueAt(i);expected=showduinoSceneFingerprint(expected,c->timeMs,c->command);}
 assert(expected==owner.sceneFingerprint());
 assert(!owner.handleStartScene(1000,0,&legacy,expected^1,owner.rt.revision));
 assert(showduinoSceneFingerprint(expected,500,"different.wav")!=owner.sceneFingerprint());
 assert(!owner.handleStartScene(500,0,&legacy,owner.sceneFingerprint(),owner.rt.revision)); // no arbitrary cue/time seek
 assert(!owner.handleStartScene(2000,0,&legacy,owner.sceneFingerprint(),owner.rt.revision)); // cannot seek beyond the story
 assert(owner.handleRun(0,&legacy));owner.service(0,&legacy);assert(fired==2);
 assert(!owner.handleStartScene(1000,0,&legacy,owner.sceneFingerprint(),owner.rt.revision)); // running guard
 uint32_t beforeEmergency=owner.rt.revision;
 gShowduinoTestMillis=500;owner.onEmergencyStop(500,&legacy);
 assert(!owner.handleStartScene(1000,500,&legacy,owner.sceneFingerprint(),owner.rt.revision));
 owner.onEmergencyCleared(500,&legacy);assert(owner.rt.state==SHOW_STATE_PAUSED);
 owner.service(500,&legacy);assert(fired==2); // clear never starts playback
 assert(!owner.handleStartScene(1000,500,&legacy,owner.sceneFingerprint(),beforeEmergency)); // reject delayed pre-emergency requests
 legacy.emergency=EmergencyState::Active;assert(!owner.handleStartScene(1000,500,&legacy,owner.sceneFingerprint(),owner.rt.revision));
 legacy.emergency=EmergencyState::Clear;
 assert(owner.handleStartScene(1000,500,&legacy,owner.sceneFingerprint(),owner.rt.revision));assert(fired==2); // selection/start positions only
 owner.service(500,&legacy);assert(fired==5);assert(!strcmp(last,"AUDIO:NODE:PLAY:dinosaur.wav"));
 assert(owner.rt.elapsedMs==1000); // scene retains production-relative clock
 gShowduinoTestMillis=750;owner.service(750,&legacy);assert(owner.rt.elapsedMs==1250&&fired==5);
 owner.onEmergencyStop(750,&legacy);assert(!owner.handleStartScene(0,750,&legacy,owner.sceneFingerprint(),owner.rt.revision));
 owner.onEmergencyCleared(750,&legacy);assert(fired==5); // repeated safety clear no dispatch
 assert(owner.handleStop(750,&legacy));assert(owner.handleStartScene(0,750,&legacy,owner.sceneFingerprint(),owner.rt.revision));
 owner.service(750,&legacy);assert(fired==7);
 // Run every actual story checkpoint through the authoritative timeline owner.
 std::ifstream input("examples/productions/time-machine-first-journey.shdo");
 std::string document((std::istreambuf_iterator<char>(input)),{});assert(!document.empty());
 static ShdoCue commands[512];ShdoManifest manifest{};uint16_t count=0;char error[128];
 assert(shdoCompile(document.c_str(),document.size(),&manifest,commands,512,&count,error,sizeof(error))==SHDO_OK);
 for(uint32_t point:{0u,18000u,44000u,74000u,100000u,130000u,156000u,172000u}){
  ShowRuntimeOwner story;ShowEngineState state;story.begin(nullptr);story.bootToIdle();story.setDispatch(dispatch);
  assert(story.handleTlBegin(false));for(unsigned i=0;i<count;++i)assert(story.handleTlCue(commands[i].timeMs,commands[i].command));
  assert(story.handleTlEnd(gShowduinoTestMillis,&state,"Time machine",false));
  story.onEmergencyStop(gShowduinoTestMillis,&state);assert(!story.canStartScene(point,&state,story.sceneFingerprint(),story.rt.revision));
  firedCommands.clear();story.onEmergencyCleared(gShowduinoTestMillis,&state);assert(firedCommands.empty());
  assert(story.handleStartScene(point,gShowduinoTestMillis,&state,story.sceneFingerprint(),story.rt.revision));assert(firedCommands.empty());
  story.service(gShowduinoTestMillis,&state);
  unsigned expectedCommands=0;bool bed=false,lamp=false;
  for(unsigned i=0;i<count;++i)if(commands[i].timeMs==point){
   assert(expectedCommands<firedCommands.size()&&firedCommands[expectedCommands]==commands[i].command);++expectedCommands;
   bed|=!strncmp(commands[i].command,"AMBIENCE:LOOP:",14);
   lamp|=strstr(commands[i].command,":FX:STEADY_FLAME")!=nullptr;
  }
  assert(expectedCommands==firedCommands.size());
  if(point<172000)assert(bed);if(point)assert(lamp);
  assert(story.rt.elapsedMs==point);
  if(point==44000 || point==100000 || point==156000 || point==172000){
   const size_t start=firedCommands.size();gShowduinoTestMillis+=2500;story.service(gShowduinoTestMillis,&state);
   bool afterIgnition=false;for(size_t i=start;i<firedCommands.size();++i)
    afterIgnition|=firedCommands[i].find(point==172000?":FX:FLARE:":":FX:UNSTABLE:")!=std::string::npos;
   assert(afterIgnition);
  }
  story.timeline.ClearTimeline();
 }
 puts("Scene recovery: emergency, explicit start, checkpoint validation, no past cue replay and clock tests passed");
}
