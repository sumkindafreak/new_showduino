const fs = require('fs'), path = require('path'), assert = require('assert');
const {spawnSync} = require('child_process');
const root = path.resolve(__dirname, '../..');
const src = p => fs.readFileSync(path.join(root, 'firmware/audio-node-esp32-a1s/ShowduinoAudioNode/src', p), 'utf8');
function fn(text, signature) {
 const start = text.indexOf(signature); assert(start >= 0, signature);
 const open = text.indexOf('{', start); let depth = 1, end = open + 1;
 for (; depth; end++) { if (text[end] === '{') depth++; else if (text[end] === '}') depth--; }
 return text.slice(start, end);
}
const codec = src('AudioCodec.cpp'), playback = src('AudioPlayback.cpp'), storage = src('AudioStorage.cpp');
const cache = path.join(root, '.host-test-cache');
fs.mkdirSync(cache, {recursive:true});
const outdir = fs.mkdtempSync(path.join(cache, 'codec-playback-'));
const volumeHeader = path.join(root, 'firmware/audio-node-esp32-a1s/ShowduinoAudioNode/src/AudioVolume.h').replaceAll('\\','/');
const harness = `
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <vector>
#include "${volumeHeader}"
static uint8_t sLastPercent=80; static bool sMuted=false, pa=false;
static char sOutput[12]="HEADPHONE"; static uint8_t regs[256]={}; static int failReg=-1;
static bool wr(uint8_t r,uint8_t v) { if(r==failReg)return false;regs[r]=v;return true; }
static void audioCodecSetPa(bool on){pa=on;}
${fn(codec, 'bool audioCodecSetVolume(')}
${fn(codec, 'void audioCodecMute(')}
static int token, *sLock=&token; static constexpr int pdTRUE=1;
static bool sMp3=false; static char sPath[128]="track.mp3";
static uint32_t sMaxSdReadUs=0,sMaxI2sWriteUs=0,sShortWrites=0; static uint32_t micros(){static uint32_t t=0;return ++t;}
static bool mp3Alive=true,mp3StartOk=true;static uint32_t mp3Pos=0;static int mp3Stops=0,mp3Starts=0;static const char*mp3Error="";
static bool audioMp3Service(){return mp3Alive;}static uint32_t audioMp3Position(){return mp3Pos;}static const char*audioMp3LastError(){return mp3Error;}static void audioMp3Stop(){mp3Stops++;}static bool audioMp3Start(const char*){mp3Starts++;return mp3StartOk;}
static bool sOpen=true,sPaused=false,sLoop=true,sI2sOn=true,sCompleted=false,sFailed=false,sGapActive=false;
static uint32_t sDataPos=4,sDataBytes=4,sDataStart=0,sDone=0,sFail=0,sUnderrun=0,sBytes=0,sRate=44100,sRepeatGapMs=8000,sGapBytesLeft=0;
static uint16_t sChannels=2; static uint8_t sBuf[4096];
static int xSemaphoreTake(int*,int){return 1;} static void xSemaphoreGive(int*){}
static void lockPlayback(){} static void unlockPlayback(){} static void setErr(const char*){}
struct MockFile { int reads=0; bool patterned=false;size_t offset=0; bool closed=false; void seek(uint32_t){} void close(){closed=true;} int read(uint8_t*b,size_t n){reads++;for(size_t i=0;i<n;i++)b[i]=patterned?uint8_t((offset+i)%251):0x55;offset+=n;return (int)n;} } sFile;
struct MockI2s { std::vector<uint8_t> bytes; size_t cap=512; bool ended=false; size_t write(uint8_t*b,size_t n){n=std::min(n,cap);bytes.insert(bytes.end(),b,b+n);return n;} void end(){ended=true;} } sI2s;
${fn(playback, 'void audioPlaybackStop(')}
${fn(playback, 'void audioPlaybackService(')}
static bool audioPlaybackActive(){return sOpen&&!sPaused;}static bool audioPlaybackPaused(){return sOpen&&sPaused;}
static bool sVolDirty=true,sWritable=true,sReady=true,sLastPresent=true,sRemovedEvt=false,sInsertedEvt=false;
static uint32_t sVolSaveAt=0,sProbeMs=0;static unsigned configWrites=0,cardPolls=0;
static uint32_t millis(){return 2000;}static bool writeConfig(){configWrites++;return true;}
static bool detectPinPresent(){return true;}static void unmountCard(const char*){sReady=false;}static bool mountCard(){return true;}
struct SdMock{unsigned cardSize(){cardPolls++;return 1024;}}SD;struct SerialMock{void println(const char*){}}Serial;
${fn(storage, 'void audioStorageLoop(')}
static void reset(uint32_t rate,uint16_t channels,uint32_t gap) {
 sMp3=false;sShortWrites=0;mp3Stops=0;mp3Starts=0;mp3Alive=true;mp3Pos=0;mp3Error="";mp3StartOk=true;
 sOpen=true;sPaused=false;sLoop=true;sI2sOn=true;sCompleted=false;sFailed=false;sGapActive=false;
 sDataPos=4;sDataBytes=4;sRate=rate;sChannels=channels;sRepeatGapMs=gap;sGapBytesLeft=0;sBytes=0;sUnderrun=0;
 sFile=MockFile();sI2s=MockI2s();
}
int main(){
 assert(audioCodecSetVolume(50));assert(regs[0x1A]==12&&regs[0x1B]==12);assert(!pa);
 assert(audioCodecSetVolume(0));assert(regs[0x19]&4);audioCodecMute(false);assert(regs[0x19]&4);
 assert(audioCodecSetVolume(50));assert(!(regs[0x19]&4));
 audioCodecMute(true);assert(audioCodecSetVolume(75));assert(regs[0x19]&4);
 strcpy(sOutput,"SPEAKER");assert(audioCodecSetVolume(80));assert(!pa);
 audioCodecMute(false);assert(pa);assert(audioCodecSetVolume(0));assert(!pa);
 failReg=0x19;assert(!audioCodecSetVolume(50));assert(!pa);failReg=-1;
 reset(44100,2,8000);sDataPos=0;audioPlaybackService();assert(sFile.reads==1&&sI2s.bytes.size()==4&&!sGapActive);
 for(uint32_t rate: {8000U,16000U,22050U,32000U,44100U,48000U})for(uint16_t channels:{1,2}){
  reset(rate,channels,8000);const size_t expected=(size_t)rate*8*4;
  for(unsigned guard=0;sFile.reads==0&&guard<10000;guard++)audioPlaybackService();
  assert(sFile.reads==1);assert(sI2s.bytes.size()==expected+(channels==1?8:4));
  assert(std::all_of(sI2s.bytes.begin(),sI2s.bytes.begin()+expected,[](uint8_t v){return v==0;}));
  assert(sI2s.bytes[expected]==0x55);assert(sUnderrun==0);
  for(unsigned guard=0;sFile.reads<2&&guard<10000;guard++)audioPlaybackService();
  assert(sFile.reads==2);assert(sI2s.bytes.size()==2*(expected+(channels==1?8:4)));
 }
 reset(44100,2,0);audioPlaybackService();assert(sFile.reads==1&&sI2s.bytes.size()==4);
 reset(44100,2,8000);sI2s.cap=128;audioPlaybackService();assert(sGapBytesLeft==44100*8*4-128);assert(sShortWrites==1&&sUnderrun==0);
 audioPlaybackStop();assert(!sGapActive&&sGapBytesLeft==0&&!sOpen);auto count=sI2s.bytes.size();audioPlaybackService();assert(sI2s.bytes.size()==count&&sFile.reads==0);
 reset(44100,2,8000);sLoop=false;audioPlaybackService();assert(sCompleted&&!sOpen&&sI2s.bytes.empty());
 reset(44100,2,0);sMp3=true;sDataPos=0;mp3Pos=2;audioPlaybackService();assert(sDataPos==2&&sBytes==2&&sOpen);
 sPaused=true;mp3Pos=4;audioPlaybackService();assert(sDataPos==2);sPaused=false;
 mp3Alive=false;mp3Pos=4;audioPlaybackService();assert(sOpen&&sDataPos==0&&mp3Starts==1);
 audioPlaybackStop();assert(!sOpen&&!sMp3&&mp3Stops==1);
 reset(44100,2,0);sMp3=true;sLoop=false;mp3Alive=false;mp3Pos=4;audioPlaybackService();assert(sCompleted&&!sOpen&&mp3Stops==1);
 reset(44100,2,0);sMp3=true;mp3Alive=false;mp3Error="bad stream";audioPlaybackService();assert(sFailed&&!sOpen&&mp3Stops==1);
 reset(44100,2,0);sMp3=true;mp3Alive=false;mp3Pos=4;mp3StartOk=false;audioPlaybackService();assert(sFailed&&!sOpen);
 reset(44100,2,0);sMp3=true;sGapActive=true;sGapBytesLeft=1024;audioPlaybackStop();audioPlaybackService();assert(!sOpen&&!sGapActive&&mp3Stops==1);
 reset(44100,2,0);sDataPos=0;sDataBytes=4096;audioPlaybackService();assert(sI2s.bytes.size()==4096&&sShortWrites>0&&sUnderrun==0);
 reset(44100,1,0);sDataPos=0;sDataBytes=2048;audioPlaybackService();assert(sI2s.bytes.size()==4096&&sUnderrun==0);
 reset(44100,2,0);sDataPos=0;sDataBytes=3;audioPlaybackService();assert(sFailed&&!sOpen&&sI2s.bytes.empty());
 reset(44100,2,0);sDataPos=0;sDataBytes=4096;sFile.patterned=true;audioPlaybackService();
 for(size_t i=0;i<4096;i++)assert(sI2s.bytes[i]==uint8_t(i%251));
 reset(44100,1,0);sDataPos=0;sDataBytes=2048;sFile.patterned=true;audioPlaybackService();
 for(size_t i=0;i<1024;i++)for(size_t c=0;c<2;c++){assert(sI2s.bytes[i*4+c*2]==uint8_t((i*2)%251));assert(sI2s.bytes[i*4+c*2+1]==uint8_t((i*2+1)%251));}
 reset(44100,2,0);sVolDirty=true;configWrites=cardPolls=0;audioStorageLoop();assert(sVolDirty&&configWrites==0&&cardPolls==0);
 sPaused=true;audioStorageLoop();assert(sVolDirty&&configWrites==0&&cardPolls==0);
 sOpen=false;sPaused=false;audioStorageLoop();assert(!sVolDirty&&configWrites==1&&cardPolls==1);
 puts("PASS: actual codec gain/mute/PA and playback eight-second silence, normal loops, stop cancellation, mono/stereo, sample rates and short writes");
}
`;
const cpp=path.join(outdir,'actual_codec_playback.cpp'), exe=path.join(outdir,'actual_codec_playback.exe');
fs.writeFileSync(cpp,harness);
for (const [command,args] of [['g++',['-std=c++17','-Wall','-Wextra',cpp,'-o',exe]],[exe,[]]]) {
 const r=spawnSync(command,args,{encoding:'utf8'});process.stdout.write(r.stdout||'');process.stderr.write(r.stderr||'');
 if(r.error)throw r.error;assert.equal(r.status,0,command);
}
