#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <type_traits>

// Inject failure only into the adapter's non-throwing array allocation.
static bool failNextArray = false;
void *operator new[](std::size_t size,const std::nothrow_t&) noexcept {
  if(failNextArray){failNextArray=false;return nullptr;}
  try{return ::operator new[](size);}catch(...){return nullptr;}
}
void operator delete[](void *p,const std::nothrow_t&) noexcept { ::operator delete[](p); }

#define SHOWDUINO_LED_GPIO 2
#include "../../firmware/shared-pixel/ShowduinoLedStrip.h"

int main(){
  static_assert(!std::is_copy_constructible<ShowduinoLedStrip>::value,"A pixel buffer has a single owner");
  {
    ShowduinoLedStrip strip(4,2,NEO_GRB+NEO_KHZ800);
    assert(!strip.ready());assert(strip.begin());assert(strip.ready());
    assert(strip.numPixels()==4);assert(strip.getBrightness()==255);
    strip.setPixelColor(0,0x123456);strip.setPixelColor(3,1,2,3);
    strip.setPixelColor(4,0xffffff);
    assert(strip.getPixelColor(0)==0x123456&&strip.getPixelColor(3)==0x010203);
    assert(strip.getPixelColor(4)==0);
    strip.setBrightness(64);strip.show();assert(strip.getBrightness()==64);
    strip.clear();for(unsigned i=0;i<4;++i)assert(strip.getPixelColor(i)==0);
    // A caller can explicitly request full white independent of a previous dim value.
    strip.setBrightness(255);for(unsigned i=0;i<4;++i)strip.setPixelColor(i,255,255,255);
    strip.show();for(unsigned i=0;i<4;++i)assert(strip.getPixelColor(i)==0xffffff);
#if SHOWDUINO_LED_USE_FASTLED
    auto *c=FastLED.byPin[2];assert(c&&c->shows==2&&c->brightness==255);
    assert(c->uncorrected&&c->untempered&&c->dither==0);assert(FastLED.globalShows==0);
    assert(strip.begin());assert(FastLED.registrations==1);
    ShowduinoLedStrip duplicate(8,2,NEO_GRB+NEO_KHZ800);
    assert(!duplicate.begin());assert(!duplicate.ready());
    { ShowduinoLedStrip another(8,2,NEO_GRB+NEO_KHZ800);assert(!another.begin()); }
    assert(c->count==4&&strip.ready()); // Failed duplicate destruction must not detach owner.
#endif
  }
#if SHOWDUINO_LED_USE_FASTLED
  assert(FastLED.byPin[2]->data==nullptr&&FastLED.byPin[2]->count==0);
  for(uint16_t n:{uint16_t(10),uint16_t(30),uint16_t(4)}){
    ShowduinoLedStrip strip(n,2,NEO_GRB+NEO_KHZ800);assert(strip.begin());
    strip.setPixelColor(n-1,9,8,7);strip.show();
    assert(FastLED.byPin[2]->count==n&&FastLED.byPin[2]->last.size()==n);
    assert(FastLED.byPin[2]->last.back().r==9);assert(FastLED.registrations==1);
  }
  {
    ShowduinoLedStrip first(4,2,NEO_GRB+NEO_KHZ800);
    ShowduinoFastLedStrip<3> second(4,3,NEO_GRB+NEO_KHZ800);
    assert(first.begin()&&second.begin());auto *a=FastLED.byPin[2],*b=FastLED.byPin[3];
    const auto old=b->shows;first.show();assert(a->shows>0&&b->shows==old);
    second.show();assert(b->shows==old+1&&FastLED.globalShows==0);
  }
  ShowduinoLedStrip wrongPin(4,3,NEO_GRB+NEO_KHZ800);assert(!wrongPin.begin());
  ShowduinoLedStrip wrongOrder(4,2,NEO_RGB+NEO_KHZ800);assert(!wrongOrder.begin());
  ShowduinoLedStrip wrongSpeed(4,2,NEO_GRB+NEO_KHZ400);assert(!wrongSpeed.begin());
  ShowduinoLedStrip zero(0,2,NEO_GRB+NEO_KHZ800);assert(!zero.begin());
  ShowduinoLedStrip retry(4,2,NEO_GRB+NEO_KHZ800);failNextArray=true;
  assert(!retry.begin()&&!retry.ready());assert(FastLED.byPin[2]->data==nullptr);
  assert(retry.begin()&&retry.ready());
  puts("PASS FastLED adapter: ownership, reuse, rebind, geometry, allocation failure, RGB, per-line refresh and explicit white");
#else
  legacyBeginFails=true;ShowduinoLedStrip failed(4,2,NEO_GRB+NEO_KHZ800);
  assert(!failed.begin()&&!failed.ready());legacyBeginFails=false;
  legacyBufferFails=true;ShowduinoLedStrip empty(4,2,NEO_GRB+NEO_KHZ800);
  assert(!empty.begin()&&!empty.ready());legacyBufferFails=false;
  ShowduinoLedStrip badPin(4,-1,NEO_GRB+NEO_KHZ800);assert(!badPin.begin());
  puts("PASS legacy adapter: API delegation, RGB, brightness, initialization and buffer failure");
#endif
}
