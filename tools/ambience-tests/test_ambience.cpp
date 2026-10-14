#include "HostConfig.h"
#include "driver/i2s_std.h"
#include "StageAmbience.h"
#include "../../protocol/showduino_ambience_command.h"
#include "../../protocol/showduino_io_command.h"
#include <cassert>
#include <iostream>
fs::FS hostSd;
bool hostSdReady=true;
// Real stereo PCM RIFF file with enough data to observe an in-flight loop.
std::vector<uint8_t> wav(){
 std::vector<uint8_t> b;
 auto text=[&](const char* p){b.insert(b.end(),p,p+4);};
 auto u16=[&](unsigned v){b.push_back(v&255);b.push_back((v>>8)&255);};
 auto u32=[&](unsigned v){u16(v&65535);u16(v>>16);};
 text("RIFF");u32(36+2048);text("WAVE");text("fmt ");u32(16);
 u16(1);u16(2);u32(44100);u32(176400);u16(4);u16(16);
 text("data");u32(2048);for(int i=0;i<1024;++i)u16(1000);return b;
}
std::string command(const char* c){char reply[256]={};assert(stageAmbienceHandleCommand(c,reply,sizeof(reply)));return reply;}
void reset(){stageAmbienceOnEmergency(false);stageAmbienceStop();hostSd.files.clear();hostSdReady=true;failI2s=false;maxWriteBytes=SIZE_MAX;writeResult=ESP_OK;stageAmbienceBegin();}
int main(){
 const char* normal="/showduino/audio/ambience/room.wav";
 const char* fallback=PATH_AUDIO_SYSTEM_LIBRARY "/emergency.wav";
 reset();hostSd.files[normal]=wav();hostSd.files[PATH_SYSTEM_EMERGENCY_WAV]=wav();
 assert(command("AMBIENCE:LOOP:/showduino/audio/ambience/room.wav")=="OK:AMBIENCE:LOOP");
 command("AMBIENCE:VOLUME:0");stageAmbienceOnEmergency(true);
 assert(stageAmbienceStatus().playing&&stageAmbienceStatus().looping&&stageAmbienceStatus().emergencyPlayback);
 assert(std::string(stageAmbienceStatus().path)==PATH_SYSTEM_EMERGENCY_WAV);
 assert(command("AMBIENCE:FILE:0")=="AMBIENCE:FILE:0:room.wav");
 assert(command("AMBIENCE:FILE:1")=="AMBIENCE:FILE:END");
 int count=allocations;stageAmbienceOnEmergency(true);assert(allocations==count);
 for(auto c:{"AMBIENCE:STOP","AMBIENCE:VOLUME:0","AMBIENCE:PLAY:/showduino/audio/ambience/room.wav"})assert(command(c)=="ERR:AMBIENCE:EMERGENCY");
 assert(command("AMBIENCE:STATUS").find("EMERGENCY:")!=std::string::npos);
 sentSamples.clear();stageAmbienceLoop();assert(!sentSamples.empty()&&sentSamples[0]==1000);
 stageAmbienceOnEmergency(false);assert(!stageAmbienceStatus().playing&&!stageAmbienceStatus().emergencyBlocked);
 assert(stageAmbienceStatus().volume==0); // User volume survives emergency override.
 command("AMBIENCE:LOOP:/showduino/audio/ambience/room.wav");assert(stageAmbienceStatus().playing);
 command("AMBIENCE:STOP");assert(!stageAmbienceStatus().playing);
 reset();hostSd.files[fallback]=wav();stageAmbienceOnEmergency(true);
 assert(stageAmbienceStatus().emergencyPlayback&&std::string(stageAmbienceStatus().path)==fallback);
 reset();hostSd.files[PATH_SYSTEM_EMERGENCY_WAV]={0,1,2,3};hostSd.files[fallback]=wav();
 stageAmbienceOnEmergency(true);assert(stageAmbienceStatus().emergencyPlayback);
 reset();stageAmbienceOnEmergency(true);assert(!stageAmbienceStatus().playing&&stageAmbienceStatus().emergencyBlocked);
 assert(command("AMBIENCE:LOOP:/showduino/audio/ambience/room.wav")=="ERR:AMBIENCE:EMERGENCY");
 reset();hostSdReady=false;stageAmbienceOnEmergency(true);assert(!stageAmbienceStatus().playing&&stageAmbienceStatus().emergencyBlocked);
 reset();hostSd.files[PATH_SYSTEM_EMERGENCY_WAV]=wav();failI2s=true;stageAmbienceOnEmergency(true);
 assert(!stageAmbienceStatus().playing&&stageAmbienceStatus().emergencyBlocked);
 stageAmbienceOnEmergency(false);
 reset();auto mono=wav();mono[22]=1;mono[28]=0x88;mono[29]=0x58;mono[30]=1;mono[31]=0;mono[32]=2;
 hostSd.files[normal]=mono;command("AMBIENCE:VOLUME:100");
 assert(command("AMBIENCE:PLAY:/showduino/audio/ambience/room.wav")=="OK:AMBIENCE:PLAY");
 sentSamples.clear();maxWriteBytes=0;stageAmbienceLoop();assert(sentSamples.empty()&&stageAmbienceStatus().playing);
 maxWriteBytes=128;for(int i=0;i<32;++i)stageAmbienceLoop();
 assert(sentSamples.size()==2048&&!stageAmbienceStatus().playing);
 for(auto sample:sentSamples)assert(sample==1000);
 assert(!showduinoAmbienceCommandAllowed("AMBIENCE:VOLUME:abc"));
 assert(!showduinoAmbienceCommandAllowed("AMBIENCE:VOLUME:101"));
 assert(!showduinoAmbienceCommandAllowed("AMBIENCE:PLAY:/showduino/audio/ambience/../system/emergency.wav"));
 assert(!showduinoAmbienceCommandAllowed("AMBIENCE:PLAY:/elsewhere.wav"));
 assert(showduinoAmbienceCommandAllowed("AMBIENCE:VOLUME:0"));
 const std::string maxName=std::string(51,'a')+".wav";
 assert(showduinoAmbienceCommandAllowed(("AMBIENCE:PLAY:/showduino/audio/ambience/"+maxName).c_str()));
 assert(!showduinoAmbienceCommandAllowed(("AMBIENCE:PLAY:/showduino/audio/ambience/a"+maxName).c_str()));
 assert(command("AMBIENCE:STATUS").size()<SHOWDUINO_DESK_COMMAND_MAX);
 assert(command("AMBIENCE:VOLUME:abc")=="ERR:AMBIENCE:INVALID_COMMAND");
 reset();hostSd.files[normal]=wav();hostSd.files[normal].pop_back();
 assert(command("AMBIENCE:PLAY:/showduino/audio/ambience/room.wav").find("TRUNCATED_WAV")!=std::string::npos);
 reset();hostSd.files[normal]=wav();command("AMBIENCE:PLAY:/showduino/audio/ambience/room.wav");
 writeResult=ESP_ERR_TIMEOUT;stageAmbienceLoop();assert(stageAmbienceStatus().playing);
 writeResult=-1;stageAmbienceLoop();assert(!stageAmbienceStatus().playing&&std::string(stageAmbienceStatus().error)=="I2S_WRITE");
 for(auto c:{"IO:STATUS","IO:ALL:OFF","IO:1:MODE:OUTPUT","IO:2:PULSE:3600000","IO:1:DEBOUNCE:0","IO:SAVE"})assert(showduinoIoCommandAllowed(c));
 for(auto c:{"IO:3:ON","IO:1:PULSE:0","IO:1:PULSE:3600001","IO:1:DEBOUNCE:5001","IO:1:ON:garbage","IO:1:PULSE:100x","IO:1:MODE:bad","IO:1:MODE:OUTPUT\nEMERGENCY:CLEAR"})assert(!showduinoIoCommandAllowed(c));
 std::cout<<"Ambience playback, emergency, inventory and browser command-policy tests passed\n";
}
