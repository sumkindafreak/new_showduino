// Compile-only API probe. It is not a node image and must not be flashed.
#define SHOWDUINO_LED_GPIO 2
#include "../../../../firmware/shared-pixel/ShowduinoLedStrip.h"
ShowduinoLedStrip strip(4,SHOWDUINO_LED_GPIO,NEO_GRB+NEO_KHZ800);
void setup(){
  if(strip.begin()){
    strip.setBrightness(255);
    strip.clear();
    strip.setPixelColor(0,ShowduinoLedStrip::Color(255,0,0));
    strip.show();
  }
}
void loop(){}
