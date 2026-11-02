#include "support.h"
#include "../../firmware/audio-node-esp32-a1s/ShowduinoAudioNode/src/AudioPixelFrame.h"
int main(){
  uint8_t rgb[]={18,52,86,255,0,1},wire[]={9,9,9,9,9,9};
  for(size_t invalid:{size_t(0),size_t(1),size_t(5)})assert(!audioPixelPackRgbToGrb(rgb,invalid,wire,sizeof(wire)));
  assert(!audioPixelPackRgbToGrb(nullptr,6,wire,6)&&!audioPixelPackRgbToGrb(rgb,6,nullptr,6)&&!audioPixelPackRgbToGrb(rgb,6,wire,5));
  assert(std::all_of(std::begin(wire),std::end(wire),[](uint8_t b){return b==9;}));
  assert(audioPixelPackRgbToGrb(rgb,6,wire,6));assert((std::vector<uint8_t>(wire,wire+6)==std::vector<uint8_t>{52,18,86,0,255,1}));
  assert(audioPixelPackRgbToGrb(rgb,6,rgb,6));assert(std::memcmp(rgb,wire,6)==0);
  AudioPixelOutput output;assert(output.begin(22,6));
  Adafruit_NeoPixel buffer(2,22,NEO_GRB+NEO_KHZ800);assert(buffer.begin());
  const uint8_t frame[]={1,2,3,0,0,0};assert(audioPixelWriteRgbFrame(output,buffer,frame,6));expectBytes(22,{2,1,3,0,0,0});
  const auto writes=channels[22].writes.size();
  assert(!audioPixelWriteRgbFrame(output,buffer,frame,3)&&!audioPixelWriteRgbFrame(output,buffer,nullptr,6));
  buffer.setBrightness(127);assert(!audioPixelWriteRgbFrame(output,buffer,frame,6));buffer.setBrightness(255);
  assert(channels[22].writes.size()==writes);
  channels[22].failWrite=true;assert(!audioPixelWriteRgbFrame(output,buffer,frame,6));
  channels[22].failWrite=false;assert(audioPixelWriteRgbFrame(output,buffer,frame,6));
  output.end();assert(outputAllocations.empty());
  puts("PASS explicit RGB/GRB boundary: order, bounds/null/size, no partial write on invalid input, exact alias, no double brightness, real failure return");
}
