#pragma once
#include <cstdint>
#include <vector>
struct CRGB {
  uint8_t r=0,g=0,b=0;
  CRGB() = default;
  CRGB(uint8_t red,uint8_t green,uint8_t blue):r(red),g(green),b(blue) {}
};
enum EOrder {GRB};
enum LEDColorCorrection {UncorrectedColor};
enum ColorTemperature {UncorrectedTemperature};
template<uint8_t GPIO,EOrder ORDER>class WS2812 {};
class CLEDController {
 public:
  CRGB *data=nullptr;int count=0;unsigned shows=0,bindings=0;uint8_t brightness=0,dither=1;
  bool uncorrected=false,untempered=false;std::vector<CRGB>last;
  CLEDController &setLeds(CRGB *p,int n){data=p;count=n;++bindings;return *this;}
  CLEDController &setCorrection(LEDColorCorrection){uncorrected=true;return *this;}
  CLEDController &setTemperature(ColorTemperature){untempered=true;return *this;}
  CLEDController &setDither(uint8_t v){dither=v;return *this;}
  void showLeds(uint8_t b){++shows;brightness=b;last.assign(data,data+count);}
};
struct FakeFastLED {
  unsigned registrations=0,globalShows=0;
  CLEDController *byPin[64]{};
  template<template<uint8_t,EOrder>class CHIPSET,uint8_t GPIO,EOrder ORDER>
  CLEDController &addLeds(CRGB *p,int n){
    static CLEDController c;++registrations;byPin[GPIO]=&c;return c.setLeds(p,n);
  }
  void show(){++globalShows;}
};
inline FakeFastLED FastLED;
