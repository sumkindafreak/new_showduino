// Exercise the actual renderer with hardware/time/audio/config doubles.
#define SHOWDUINO_LOG_H
#include <cassert>
#include <cstdio>
#include <Arduino.h>
#ifndef LAMP_ENGINE_SOURCE
#define LAMP_ENGINE_SOURCE "../../firmware/s3-lamp-node/ShowduinoS3LampNode/src/LampEngine.cpp"
#endif
#include LAMP_ENGINE_SOURCE

static uint8_t savedBrightness=60;
static ShowduinoLampSound sound=SHOWDUINO_LAMP_SND_NONE;
uint8_t lampConfigBrightness(){return savedBrightness;}
void lampConfigSetBrightness(uint8_t v){savedBrightness=v;}
uint8_t lampConfigFlameActivity(){return 65;}
uint8_t lampConfigFlickerAmount(){return 45;}
uint8_t lampConfigIgnitionSpeed(){return 100;}
uint8_t lampConfigJewelCore(){return 3;}
void lampAudioPlay(ShowduinoLampSound v){sound=v;}
void lampAudioStop(){sound=SHOWDUINO_LAMP_SND_NONE;}

static void sample(){
  assert(lampEngineCount()==7 && lampEnginePin()==8 && lampEngineJewelReady());
  printf("%u,%d,%d,%d,%d,%d",testNow,int(lampEngineCarbide()),
         int(lampEngineVisual()),int(sound),lampEngineEmergency(),lampEngineBrightness());
  for(unsigned i=0;i<7;++i){
    uint8_t r,g,b;lampEnginePixelRgb(i,&r,&g,&b);
    assert(sStrip->getPixelColor(i)==((uint32_t(r)<<16)|(uint32_t(g)<<8)|b));
#if SHOWDUINO_LAMP_USE_FASTLED
    const auto *c=FastLED.byPin[8];assert(c&&c->count==7&&c->brightness==255);
    assert(c->last[i].r==r&&c->last[i].g==g&&c->last[i].b==b);
    assert(FastLED.globalShows==0);
#endif
    printf(",%02x%02x%02x",r,g,b);
  }
  puts("");
}
static void step(unsigned ticks){for(unsigned i=0;i<ticks;++i){testNow+=25;lampEngineService();sample();}}
static void assertWhite(){for(unsigned i=0;i<7;++i)assert(sStrip->getPixelColor(i)==0xffffff);}
static void assertBlack(){for(unsigned i=0;i<7;++i)assert(sStrip->getPixelColor(i)==0);}
int main(){
  lampEngineBlackoutEarly();lampEngineBegin();assertBlack();sample();
  lampEngineSetSolid(120,80,40);step(2);
  for(uint8_t bri:{uint8_t(0),uint8_t(1),uint8_t(37),uint8_t(100)}){
    lampEngineSetBrightness(bri);step(2);
    assert(sStrip->getPixelColor(0)==sStrip->Color(120*bri/100,80*bri/100,40*bri/100));
  }
  // Walk every existing effect ID through the unmodified renderer.
  for(int fx=0;fx<SHOWDUINO_LAMP_FX_COUNT;++fx){
    lampEngineSetCompatFx(static_cast<ShowduinoLampFx>(fx),43,67);step(12);
  }
  lampEngineApplyEvent(SHOWDUINO_CARBIDE_EV_FORCE_OFF);step(2);
  lampEngineApplyEvent(SHOWDUINO_CARBIDE_EV_IGNITE);step(300);
  lampEngineSetBlowStress(70);step(40);lampEngineSetBlowStress(0);step(80);
  assert(lampEngineStartIdentify());step(80);
  assert(lampEngineStartJewelTest());step(42);
  // Emergency preempts each interactive mode even at saved brightness zero.
  for(int mode=0;mode<3;++mode){
    lampEngineSetBrightness(0);
    if(mode==0)lampEngineSetSolid(1,2,3);
    if(mode==1)assert(lampEngineStartIdentify());
    if(mode==2)assert(lampEngineStartJewelTest());
    lampEngineOnEmergency(true);assertWhite();sample();
    assert(!lampEngineStartIdentify()&&!lampEngineStartJewelTest());
    lampEngineSetSolid(0,0,0);lampEngineApplyEvent(SHOWDUINO_CARBIDE_EV_IGNITE);
    lampEngineSetCompatFx(SHOWDUINO_LAMP_FX_SOLID,50,50);step(4);assertWhite();
    lampEngineOnEmergency(false);assertBlack();
    assert(sound==SHOWDUINO_LAMP_SND_NONE&&!lampEngineActive());step(4);assertBlack();
  }
}
