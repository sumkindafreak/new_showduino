#pragma once
#include <Arduino.h>
#include <esp32-hal-rmt.h>
#include <map>
#include <cassert>
inline bool failNextOutputAllocation=false;
inline std::map<void*,size_t> outputAllocations;
inline void* outputCalloc(size_t n,size_t size){
  if(failNextOutputAllocation){failNextOutputAllocation=false;return nullptr;}
  void* p=std::calloc(n,size);if(p)outputAllocations[p]=n*size;return p;
}
inline void outputFree(void* p){if(p){assert(outputAllocations.erase(p)==1);std::free(p);}}
inline unsigned vendorInitializations=0,vendorReleaseCalls=0;
// Compile the real vendor ESP32 buffer code. Only its hardware entry points
// are doubled. Audio data must go through AudioPixelOutput, not vendor show().
extern "C" inline void espInit(){++vendorInitializations;}
extern "C" inline void espShow(uint16_t,uint8_t*,uint32_t n,uint8_t){assert(n==0);++vendorReleaseCalls;}
inline std::vector<uint8_t> decode(const WriteAttempt& a){
  assert(a.timeout==80&&!a.symbols.empty()&&(a.symbols.size()-1)%8==0);
  const auto& reset=a.symbols.back();
  assert(reset.level0==0&&reset.level1==0&&reset.duration0==600&&reset.duration1==600);
  std::vector<uint8_t> bytes;
  for(size_t base=0;base+1<a.symbols.size();base+=8){
    uint8_t v=0;
    for(unsigned bit=0;bit<8;++bit){const auto& s=a.symbols[base+bit];
      assert(s.level0==1&&s.level1==0);
      assert((s.duration0==8&&s.duration1==4)||(s.duration0==4&&s.duration1==8));
      v=static_cast<uint8_t>((v<<1)|(s.duration0==8?1:0));
    }bytes.push_back(v);
  }return bytes;
}
inline void expectBytes(int pin,const std::vector<uint8_t>& expected){
  assert(channels[pin].hz==10000000&&!channels[pin].writes.empty());
  const auto& a=channels[pin].writes.back();assert(a.success);assert(decode(a)==expected);
}
inline size_t errorLogs(){return std::count_if(serialLines.begin(),serialLines.end(),[](const std::string& s){return s.find("[PIXEL][ERROR]")!=std::string::npos;});}
// Canonical complete attempt/symbol/log trace for original/candidate comparison.
// Captured by the runner, hashed and optionally saved; not printed as CI noise.
inline void dumpAudioTrace(){
  for(unsigned pin=0;pin<40;++pin){const auto& c=channels[pin];
    if(!c.inits&&!c.deinits&&c.writes.empty())continue;
    printf("CHANNEL %u %u %u %u %u\n",pin,c.active,c.inits,c.deinits,c.hz);
    for(const auto& w:c.writes){printf("WRITE %u %u %u ",pin,w.success,w.timeout);
      for(const auto& s:w.symbols)printf("%u,%u,%u,%u;",unsigned(s.level0),unsigned(s.duration0),unsigned(s.level1),unsigned(s.duration1));
      puts("");
    }
  }
  for(const auto& line:serialLines)printf("SERIAL %zu:%s\n",line.size(),line.c_str());
  printf("VENDOR %u %u\n",vendorInitializations,vendorReleaseCalls);
}
