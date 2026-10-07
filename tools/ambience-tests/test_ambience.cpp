#include "HostConfig.h"
#include "driver/i2s_std.h"
#include "StageAmbience.h"
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
void reset(){stageAmbienceOnEmergency(false);stageAmbienceStop();hostSd.files.clear();hostSdReady=true;failI2s=false;stageAmbienceBegin();}
int main(){
 const char* normal="/showduino/audio/ambience/room.wav";
 const char* fallback=PATH_AUDIO_SYSTEM_LIBRARY "/emergency.wav";
 reset();hostSd.files[normal]=wav();hostSd.files[PATH_SYSTEM_EMERGENCY_WAV]=wav();
 assert(command("AMBIENCE:LOOP:/showduino/audio/ambience/room.wav")=="OK:AMBIENCE:LOOP");
 command("AMBIENCE:VOLUME:0");stageAmbienceOnEmergency(true);
 assert(stageAmbienceStatus().playing&&stageAmbienceStatus().looping&&stageAmbienceStatus().emergencyPlayback);
 assert(std::string(stageAmbienceStatus().path)==PATH_SYSTEM_EMERGENCY_WAV);
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
 std::cout<<"Ambience emergency transition, fallback, failure, override and clear tests passed\n";
}
