#define SHOWDUINO_LOG_H
#include <cassert>
#include <cstdio>
#include <cstring>
#ifndef AMBIENT_ENGINE_SOURCE
#define AMBIENT_ENGINE_SOURCE "../../firmware/director-esp32-8048s050/ShowduinoDirector8048S050/DirectorAmbientPixels.cpp"
#endif
#include AMBIENT_ENGINE_SOURCE
static void sample(){
  assert(directorAmbientReady()&&sStrip->numPixels()==2);
  printf("%u,%d,%d,%d,%d,%d",testNow,int(directorAmbientMode()),
         directorAmbientEnabled(),directorAmbientBrightness(),
         directorAmbientPresentationHeld(),directorAmbientLocatorActive());
  for(unsigned i=0;i<2;++i){
    const auto rgb=sStrip->getPixelColor(i);printf(",%06x",unsigned(rgb));
#if SHOWDUINO_DIRECTOR_AMBIENT_USE_FASTLED
    const auto *c=FastLED.byPin[17];assert(c&&c->count==2&&c->brightness==255);
    const auto &p=c->last[i];assert(rgb==((uint32_t(p.r)<<16)|(uint32_t(p.g)<<8)|p.b));
    assert(FastLED.globalShows==0);
#endif
  }
  puts("");
}
static void step(unsigned ticks){for(unsigned i=0;i<ticks;++i){testNow+=40;directorAmbientLoop(testNow);sample();}}
static void white(){for(unsigned i=0;i<2;++i)assert(sStrip->getPixelColor(i)==0xffffff);}
int main(int argc,char**argv){
  directorAmbientBegin();sample();
  if(argc>1&&strcmp(argv[1],"--emergency-locator")==0){
    directorAmbientHoldPresentation(false);directorAmbientSetBrightness(0);
    directorAmbientSetEnabled(false);directorAmbientStartLocator(testNow);
    directorAmbientSync(LINK_READY,SHOW_STATE_EMERGENCY_STOP,true,true);
    // The old locator reaches an OFF phase and displaces Emergency white.
    for(unsigned i=0;i<50;++i){testNow+=40;directorAmbientLoop(testNow);white();}
    directorAmbientStartLocator(testNow);step(5);white();
    assert(!directorAmbientLocatorActive());
    return 0;
  }
  step(40);directorAmbientSetState(DIRECTOR_AMBIENT_RUNNING);step(3);
  assert(directorAmbientMode()==DIRECTOR_AMBIENT_BOOT);
  directorAmbientHoldPresentation(false);
  for(int mode=DIRECTOR_AMBIENT_OFF;mode<=DIRECTOR_AMBIENT_DEPLOYING;++mode){
    directorAmbientSetState(static_cast<DirectorAmbientMode>(mode));step(40);
  }
  for(uint8_t brightness:{uint8_t(0),uint8_t(1),uint8_t(37),uint8_t(255)}){
    directorAmbientSetBrightness(brightness);directorAmbientSetState(DIRECTOR_AMBIENT_RUNNING);step(25);
    directorAmbientSync(LINK_READY,SHOW_STATE_EMERGENCY_STOP,true,true);step(3);white();
    directorAmbientSync(LINK_READY,SHOW_STATE_IDLE,false,true);step(20);
  }
  for(int state=SHOW_STATE_BOOTING;state<=SHOW_STATE_ERROR;++state){
    for(int link=LINK_SEARCHING;link<=LINK_DISCONNECTED;++link){
      directorAmbientSync(link,static_cast<ShowState>(state),false,link==LINK_READY);step(20);
    }
  }
  directorAmbientSync(LINK_READY,SHOW_STATE_IDLE,false,true);
  for(int event=DIRECTOR_AMBIENT_EVT_NODE_DISCOVERED;event<=DIRECTOR_AMBIENT_EVT_CONNECTION_RESTORED;++event){
    directorAmbientPulse(static_cast<DirectorAmbientEvent>(event));step(35);
    directorAmbientSync(LINK_READY,SHOW_STATE_IDLE,false,true);
  }
  directorAmbientSetEnabled(false);step(20);directorAmbientSetEnabled(true);step(20);
  directorAmbientStartLocator(testNow);step(60);directorAmbientStopLocator();step(20);
  directorAmbientSync(LINK_SEARCHING,SHOW_STATE_IDLE,false,false,true,false);step(20);
  directorAmbientSync(LINK_READY,SHOW_STATE_IDLE,false,true,false,true);step(20);
}
