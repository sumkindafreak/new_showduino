#include <cassert>
#include <string>
#include <cstdio>
#include <fstream>
#include "../../protocol/showduino_shdo.h"
#include "../../firmware/stage-engine-p4/ShowduinoStageEngineP4/src/ProductionFormat.h"
static ShdoCue cues[512];
static ShdoStatus compile(const std::string &file,bool loop=true,unsigned duration=2000){
 std::string json=R"({"schema":"showduino-production-v2","package":{"version":2},"project":{"id":"ambience_test","name":"Ambience"},"devices":[{"id":"p4-bed","type":"p4-ambience","binding":{"route":"p4-ambience","nodeId":"p4"}}],"clips":[{"id":"bed","type":"audio","targetDeviceId":"p4-bed","startMs":1000,"durationMs":)"+std::to_string(duration)+R"(,"action":{"type":"audio","params":{"file":")"+file+R"(","volume":35,"loop":)"+(loop?"true":"false")+"}}}]}";
 ShdoManifest m{};uint16_t n=0;char err[128]{};
 auto status=shdoCompile(json.c_str(),json.size(),&m,cues,512,&n,err,sizeof(err));
 if(status==SHDO_OK){assert(n==(duration?3:2));assert(!strcmp(cues[0].command,"AMBIENCE:VOLUME:35"));assert(cues[0].timeMs==1000);if(duration){assert(!strcmp(cues[2].command,"AMBIENCE:STOP"));assert(cues[2].timeMs==1000+duration);}
 char timeline[8192];auto len=shdoWriteTimelineJson(timeline,sizeof(timeline),cues,n);ProductionCue restored[8];ProductionTimeline t{};ProductionParseResult result{};
 assert(productionParseTimeline(timeline,len,restored,8,&t,&result));assert(t.cueCount==n);
 for(unsigned i=0;i<n;++i){assert(!strcmp(cues[i].command,restored[i].command));assert(!strcmp(restored[i].type,"AMBIENCE"));}}
 return status;
}
int main(){
 assert(compile("whales.wav")==SHDO_OK);assert(!strcmp(cues[1].command,"AMBIENCE:LOOP:/showduino/audio/ambience/whales.wav"));
 assert(compile("/showduino/audio/ambience/electric.wav",false,0)==SHDO_OK);assert(!strncmp(cues[1].command,"AMBIENCE:PLAY:",14));
 assert(compile("../system/emergency.wav")==SHDO_UNSUPPORTED_ACTION);
 assert(compile("/showduino/audio/system/boot.wav")!=SHDO_OK);
 assert(compile("bad.WAV")==SHDO_UNSUPPORTED_ACTION);
 assert(compile("a_very_long_ambience_filename_that_would_truncate.wav")==SHDO_COMMAND_TOO_LONG);
 for(auto command:{"AMBIENCE:FILE:0","AMBIENCE:STATUS","AMBIENCE:VOLUME:101","AMBIENCE:PLAY:/showduino/audio/system/boot.wav","AUDIO:NODE:STOP"}){
  assert(!showduinoAmbienceTimelineCommandAllowed(command));
  std::string json=R"({"formatVersion":1,"cues":[{"id":"bad","timeMs":0,"type":"AMBIENCE","command":")"+std::string(command)+"\"}]}";
  ProductionCue c[1];ProductionTimeline t{};ProductionParseResult result{};assert(!productionParseTimeline(json.c_str(),json.size(),c,1,&t,&result));
 }
 // Regression: overlapping lights and ambience must produce chronological persistent cues.
 std::ifstream input("examples/productions/time-machine-first-journey.shdo");
 std::string journey((std::istreambuf_iterator<char>(input)),{});assert(!journey.empty());
 ShdoManifest manifest{};uint16_t count=0;char error[128];
 assert(shdoCompile(journey.c_str(),journey.size(),&manifest,cues,512,&count,error,sizeof(error))==SHDO_OK);
 assert(count==418);
 for(unsigned i=1;i<count;++i)assert(cues[i-1].timeMs<=cues[i].timeMs);
 static char json[131072];static ProductionCue restored[512];ProductionTimeline tl{};ProductionParseResult result{};
 auto len=shdoWriteTimelineJson(json,sizeof(json),cues,count);
 assert(productionParseTimeline(json,len,restored,512,&tl,&result));assert(tl.cueCount==count);
 unsigned scenes=0;
 for(unsigned i=0;i<count;++i)if(strstr(restored[i].command,":SCENE "))++scenes;
 assert(scenes==8);
 assert(showduinoAmbienceTimelineCommandAllowed("AMBIENCE:STOP"));puts("Timed ambience compile, persistent roundtrip and rejection tests passed");
}
