const fs=require('fs'),path=require('path'),assert=require('assert'),{spawnSync}=require('child_process');
const root=path.resolve(__dirname,'../..');
const source=fs.readFileSync(path.join(root,'firmware/audio-node-esp32-a1s/ShowduinoAudioNode/src/AudioMp3Playback.cpp'),'utf8').replace(/^#include .*$/gm,'');
const cache=path.join(root,'.host-test-cache');fs.mkdirSync(cache,{recursive:true});
const dir=fs.mkdtempSync(path.join(cache,'mp3-adapter-'));
const cpp=path.join(dir,'actual_mp3_adapter.cpp'),exe=path.join(dir,'actual_mp3_adapter.exe');
fs.writeFileSync(cpp,`
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include <vector>
#include <algorithm>
#define SHOWDUINO_AUDIO_I2S_BCLK 27
#define SHOWDUINO_AUDIO_I2S_WS 25
#define SHOWDUINO_AUDIO_I2S_DOUT 26
#define SHOWDUINO_AUDIO_I2S_DIN 35
#define SHOWDUINO_AUDIO_I2S_MCLK 0
static constexpr int I2S_MODE_STD=0,I2S_DATA_BIT_WIDTH_16BIT=0,I2S_SLOT_MODE_STEREO=0;
struct SerialMock{template<class...T>void printf(const char*,T...){}}Serial;
struct EspMock{unsigned getFreeHeap(){return 200000;}}ESP;
static std::vector<uint8_t> emitted;static size_t writeCap=4096;static bool i2sBeginOk=true;static int i2sEnds=0;
class I2SClass{public:void setPins(int,int,int,int,int){}bool begin(int,uint32_t,int,int){return i2sBeginOk;}size_t write(const uint8_t*b,size_t n){n=std::min(n,writeCap);emitted.insert(emitted.end(),b,b+n);return n;}void end(){i2sEnds++;}};
class AudioOutput{public:virtual ~AudioOutput(){}virtual bool begin(){return true;}virtual bool SetRate(int){return true;}virtual bool SetChannels(int){return true;}virtual bool ConsumeSample(int16_t*){return true;}virtual uint16_t ConsumeSamples(int16_t*,uint16_t){return 0;}virtual void flush(){}virtual bool stop(){return true;}};
static unsigned totalFrames=5000;static bool sourceOpen=true,decoderBeginOk=true,offerFormat=true;
class AudioFileSourceSD{public:explicit AudioFileSourceSD(const char*){}bool isOpen(){return sourceOpen;}unsigned getSize(){return 16000;}unsigned getPos(){return 16000;}void close(){}};
class AudioGeneratorMP3{AudioOutput*out=nullptr;unsigned frames=0;bool running=false;public:
 bool begin(AudioFileSourceSD*,AudioOutput*o){out=o;frames=0;running=decoderBeginOk;if(!running)return false;return o->begin();}
 bool isRunning(){return running;}bool stop(){running=false;if(out)out->stop();return true;}
 bool loop(){if(offerFormat){out->SetRate(44100);out->SetChannels(2);}else {running=false;return false;}
  unsigned guard=0;while(frames<totalFrames){assert(++guard<10000);int16_t sample[2]={(int16_t)frames,(int16_t)(-int(frames))};if(!out->ConsumeSample(sample))return running;frames++;}
  running=false;return false;}
};
void audioMp3Stop();
${source}
int main(){
 emitted.clear();assert(audioMp3Start("test.mp3"));assert(audioMp3Service());assert(emitted.size()==4096);
 // A service returns before EOF. STOP must acquire control and discard, never drain.
 auto before=emitted.size();audioMp3Stop();assert(emitted.size()==before&&sSource==nullptr&&sDecoder==nullptr);
 // Partial PCM writes preserve every left/right sample and naturally drain the tail.
 writeCap=128;totalFrames=2500;emitted.clear();assert(audioMp3Start("test.mp3"));unsigned calls=0;
 while(audioMp3Service()){assert(++calls<8);}assert(!audioMp3LastError()[0]);assert(emitted.size()==2500*4);
 for(unsigned i=0;i<2500;i++){int16_t pair[2];memcpy(pair,emitted.data()+i*4,4);assert(pair[0]==int16_t(i)&&pair[1]==int16_t(-int(i)));}
 audioMp3Stop();
 // STOP with a partially filled staging block must not call the write path.
 emitted.clear();writeCap=4096;sOutput.reset();sOutput.begin();sOutput.SetRate(44100);sOutput.beginService();
 int16_t sample[2]={123,-456};for(int i=0;i<3;i++)assert(sOutput.ConsumeSample(sample));assert(emitted.empty());
 audioMp3Stop();assert(emitted.empty());
 // Mono duplicates the left sample rather than reading an absent right channel.
 sOutput.begin();sOutput.SetRate(22050);sOutput.SetChannels(1);sOutput.beginService();assert(sOutput.ConsumeSample(sample));sOutput.flush();
 int16_t mono[2];memcpy(mono,emitted.data(),4);assert(mono[0]==123&&mono[1]==123);audioMp3Stop();
 // Bad source/start and failed output never leave live decoder/source handles.
 sourceOpen=false;assert(!audioMp3Start("missing.mp3"));assert(!sSource&&!sDecoder);sourceOpen=true;
 decoderBeginOk=false;assert(!audioMp3Start("bad.mp3"));assert(!sSource&&!sDecoder);decoderBeginOk=true;
 offerFormat=false;assert(audioMp3Start("garbage.mp3"));assert(!audioMp3Service());assert(strstr(audioMp3LastError(),"no decoded audio"));audioMp3Stop();offerFormat=true;
 emitted.clear();i2sBeginOk=false;assert(audioMp3Start("test.mp3"));assert(!audioMp3Service());assert(audioMp3LastError()[0]);audioMp3Stop();i2sBeginOk=true;
 sOutput.begin();assert(!sOutput.SetRate(96000));audioMp3Stop();
 emitted.clear();writeCap=0;assert(audioMp3Start("test.mp3"));assert(!audioMp3Service());assert(strstr(audioMp3LastError(),"write failed"));audioMp3Stop();
 puts("PASS: actual MP3 adapter bounded service, lossless partial writes, natural EOF drain, STOP discard, mono and failure cleanup (decoder/I2S mocked)");
}
`);
for(const [cmd,args]of [['g++',['-std=c++17','-Wall','-Wextra',cpp,'-o',exe]],[exe,[]]]){const r=spawnSync(cmd,args,{encoding:'utf8'});process.stdout.write(r.stdout||'');process.stderr.write(r.stderr||'');if(r.error)throw r.error;assert.equal(r.status,0,cmd);}
