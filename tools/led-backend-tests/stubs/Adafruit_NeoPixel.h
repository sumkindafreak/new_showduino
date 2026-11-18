#pragma once
#include <cstdint>
#include <vector>
using neoPixelType = uint16_t;
constexpr neoPixelType NEO_GRB = 0x52, NEO_RGB = 0x06, NEO_KHZ800 = 0, NEO_KHZ400 = 0x100;
inline bool legacyBeginFails = false, legacyBufferFails = false;
class Adafruit_NeoPixel {
 public:
  Adafruit_NeoPixel(uint16_t n,int16_t pin,neoPixelType):pin_(pin),pixels_(legacyBufferFails ? 0 : n) {}
  bool begin() { return pin_ >= 0 && !legacyBeginFails; }
  uint16_t numPixels() const { return static_cast<uint16_t>(pixels_.size()); }
  void setBrightness(uint8_t b) { brightness_ = b; }
  uint8_t getBrightness() const { return brightness_; }
  static uint32_t Color(uint8_t r,uint8_t g,uint8_t b) { return (uint32_t(r)<<16)|(uint32_t(g)<<8)|b; }
  void setPixelColor(uint16_t i,uint32_t rgb) { if(i<pixels_.size())pixels_[i]=rgb; }
  void setPixelColor(uint16_t i,uint8_t r,uint8_t g,uint8_t b) { setPixelColor(i,Color(r,g,b)); }
  uint32_t getPixelColor(uint16_t i) const { return i<pixels_.size()?pixels_[i]:0; }
  void clear() { for(auto &p:pixels_)p=0; }
  void show() { ++shows; }
  unsigned shows=0;
 private:
  int16_t pin_;uint8_t brightness_=255;std::vector<uint32_t>pixels_;
};
