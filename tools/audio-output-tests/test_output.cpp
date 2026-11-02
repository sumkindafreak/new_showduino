#include "support.h"
#include "../../firmware/audio-node-esp32-a1s/ShowduinoAudioNode/src/AudioPixelOutput.h"
#include <Adafruit_NeoPixel.h>
int main(){
  AudioPixelOutput status,show;std::vector<uint8_t> raw(1536);
  assert(!show.write(raw.data(),raw.size()));
  assert(!show.begin(22,0)&&!show.begin(22,1537)&&outputAllocations.empty());
  failNextOutputAllocation=true;assert(!show.begin(22,30));assert(!channels[22].active&&outputAllocations.empty());
  channels[22].failInit=true;assert(!show.begin(22,30));assert(outputAllocations.empty());channels[22].failInit=false;
  assert(status.begin(5,3));const uint8_t statusBytes[]={0x34,0x12,0x56};assert(status.write(statusBytes,3));
  const auto statusWrites=channels[5].writes.size();const auto statusDeinits=channels[5].deinits;
  for(unsigned count:{10U,30U,512U,10U}){
    assert(show.begin(22,count*3));Adafruit_NeoPixel pixels(count,22,NEO_GRB+NEO_KHZ800);
    assert(pixels.begin());
    std::vector<uint8_t> expected;
    for(unsigned i=0;i<count;++i){const uint8_t r=i,g=255-i,b=i^0xa5;
      pixels.setPixelColor(i,r,g,b);expected.insert(expected.end(),{g,r,b});}
    assert(show.write(pixels.getPixels(),count*3));expectBytes(22,expected);
    assert(channels[5].active&&channels[5].writes.size()==statusWrites&&channels[5].deinits==statusDeinits);
    const auto writes=channels[22].writes.size();
    assert(!show.write(nullptr,count*3)&&!show.write(raw.data(),count*3-1)&&!show.write(raw.data(),count*3+1));
    assert(channels[22].writes.size()==writes);
  }
  // Exhaustive byte patterns establish MSB-first bit encoding independent of RGB packing.
  assert(show.begin(22,256));for(unsigned i=0;i<256;++i)raw[i]=i;
  assert(show.write(raw.data(),256));expectBytes(22,std::vector<uint8_t>(raw.begin(),raw.begin()+256));
  auto logs=errorLogs();channels[22].failWrite=true;testNow=10;
  assert(!show.write(raw.data(),256)&&errorLogs()==logs+1);
  testNow=999;assert(!show.write(raw.data(),256)&&errorLogs()==logs+1);
  testNow=1010;assert(!show.write(raw.data(),256)&&errorLogs()==logs+2);
  channels[22].failWrite=false;assert(show.write(raw.data(),256));
  // Even an invalid re-begin tears down the old binding: preserve this observed contract.
  assert(!show.begin(22,0)&&!channels[22].active&&channels[5].active);
  assert(show.begin(22,3));assert(show.write(statusBytes,3));
  show.end();show.end();assert(channels[5].active);status.end();assert(outputAllocations.empty());
  puts("PASS actual output + real Adafruit: GRB, 10/30/512/rebind, all 256 byte patterns, reset/timeout, failures/retry, independent lines");
}
