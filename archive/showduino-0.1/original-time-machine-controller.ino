#include <SerialMP3Player.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <CuteBuzzerSounds.h>
//#include <ArduinoRS485.h>  // the ArduinoDMX library depends on ArduinoRS485
//#include <ArduinoDMX.h>


// I2C LCD setup
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Change 0x27 to your LCD I2C address if different

/* track listings for ambient player

1 - victorian london
2 - machine ambience
3 - guns
4 - warning
5 - tension
6 - rainforest
7 - electric
8 - machine travelling
9 - machine break
10 - radio 1
11 - radio 2
12 - radio 3
13 - radio 4
14 - radio 5
15 - radio 6
16 - radio 7
17   - radio 8
18   - radio 9 
19   - radio 10
20    - dinosaur roar
21    - machine boot up
 22   - whales

*/

// Pin definitions for MP3 players
#define TX 52
#define RX 51


#define TE 49
#define RE 48

SerialMP3Player mp3_ambient(RX, TX);
SerialMP3Player mp3_machine(RE, TE);

/* track listings for ambient player

1 - MACHNE AMBIENCE
2- VICTORIAN LONDON
3- WHALES
4- 
5 -MACHINE BOOT UP
6 - DINO ROAR
7 - radio - POWER BACK
8 - radio - losing control/power
9 - radio - paradox lock fail
10 - radio - 5010 pressure readings
11 - radio - 0000
12 - radio - close blast shield
13 - radio - remote control
14 - radio - remain in contact
15 - MACHINE FAIL 
16 - TRAVELLING 
17 - GUNS
18 - WARNING
19 - ELECTRIC
20 - RAINFOREST
21 - TENTION
22 - forest
23
24
25


*/

// pin 10 - blank neopixel


// Pin definitions for NeoPixel
#define PIN 9
#define NUMmachine 100
Adafruit_NeoPixel machine(NUMmachine, PIN, NEO_GRB + NEO_KHZ800);

#define NUMBERS 8
#define DISP 100
Adafruit_NeoPixel time_display(DISP, NUMBERS, NEO_GRB + NEO_KHZ800);

#define CAN 7
#define NUM_CANDLES 3
#define NUM_PIXELS_PER_CANDLE 3
#define NUM_PIXELS (NUM_CANDLES * NUM_PIXELS_PER_CANDLE)
Adafruit_NeoPixel pixels(NUM_PIXELS, CAN, NEO_GRB + NEO_KHZ800);

// Definitions for the additional independent LEDs
#define NUM_EXTRA_PIXELS 6  // Number of independent extra pixels
Adafruit_NeoPixel spotlights(NUM_EXTRA_PIXELS, CAN, NEO_GRB + NEO_KHZ800);


// Which pin on the Arduino is connected to the time circuits?
#define circuits 6
// How many Neomachine are attached to the Arduino?
#define circ 100  // Popular NeoPixel ring size
Adafruit_NeoPixel time_circuits(circ, circuits, NEO_GRB + NEO_KHZ800);


// Which pin on the Arduino is connected to the time circuits?
#define indicators 5
// How many Neomachine are attached to the Arduino?
#define indicate 100  // Popular NeoPixel ring size
Adafruit_NeoPixel Display(indicate, indicators, NEO_GRB + NEO_KHZ800);



// Button pin definitions
const int startbutton = 23;
const int emergency_stop = 24;
const int resetButton = 25;

// Scene pin led definitions
const int SC1 = 26;
const int SC2 = 27;
const int SC3 = 28;
const int SC4 = 29;
const int SC5 = 30;
const int SC6 = 31;
const int SC7 = 32;
const int SC8 = 33;

// relay pin definitions

const int relay1 = 34;  //relay1
const int relay2 = 35;  //relay2
const int relay3 = 36;  //relay3
const int relay4 = 37;  //relay4
const int relay5 = 38;  //relay1
const int relay6 = 39;  //relay2
const int relay7 = 40;  //relay3
const int relay8 = 41;  //relay4


// extras pin definitions
#define BUZZER_PIN 42

const int ONESHOT1 = 43;
const int ONESHOT2 = 44;
const int ONESHOT3 = 45;
const int ONESHOT4 = 46;




// Timing variables
unsigned long startMillis = 0;
bool machineStarted = false;
bool emergencyStopActive = false;
bool scene1 = false;
bool scene2 = false;
bool scene3 = false;
bool scene4 = false;
bool scene5 = false;
bool scene6 = false;
bool scene7 = false;
bool scene8 = false;
bool scene9 = false;
bool scene10 = false;

bool scene1Active = false;
bool scene2Active = false;
bool scene2_1active = false;
bool scene3Active = false;
bool scene4Active = false;
bool scene5Active = false;
bool scene6Active = false;
bool scene7Active = false;
bool scene8Active = false;
bool scene9Active = false;
bool scene10Active = false;

bool scene2_1 = false;
bool scene6_1 = false;

bool waiting = false;
bool waitingActive = false;


bool isshocking = false;
bool bombs = false;
bool circuit_flick = false;
bool istravelling = false;
bool isglitching = false;

// Time flashing variables

bool flash1Printed = false;
bool flash2Printed = false;
bool flash3Printed = false;
bool flash4Printed = false;
bool flash5Printed = false;
bool flash6Printed = false;

bool Printed1 = false;
bool Printed2 = false;
bool Printed3 = false;
bool Printed4 = false;
bool Printed5 = false;
bool Printed6 = false;
bool Printed7 = false;
bool Printed8 = false;
bool Printed9 = false;



bool flash7Printed = false;
bool flash8Printed = false;
bool flash9Printed = false;

bool flash10Printed = false;
bool flash11Printed = false;
bool flash12Printed = false;


bool sceneWaiting = true;
bool scene3Waiting = false;
bool scene4Waiting = false;
bool scene5Waiting = false;
bool scene6Waiting = false;
bool scene7Waiting = false;
bool scene8Waiting = false;
bool scene9Waiting = false;
bool scene10Waiting = false;


const long blink_interval = 500;  // Blink interval in milliseconds
bool isOn = false;                // State of the pixel (ON or OFF)

bool ambient_tracks[30] = { false };  // Initialize all to false
bool machine_tracks[30] = { false };  // Initialize all to false

bool oneshot1_on = false;
bool oneshot2_on = false;
bool oneshot3_on = false;
bool oneshot4_on = false;




unsigned long previousMillis = 0;
unsigned long interval = random(50, 200);  // Random interval between updates

int twinkle = random(100, 2000);  // Change the flash speed randomly
int fliker = random(600, 1200);   // Change the flash speed randomly
int timefliker = 15;
int electric = random(600, 1200);  // Change the flash speed randomly
const long fade_time = 10000;      //console fade in time


int flickerSpeed = 400;  // Speed of the flicker in milliseconds

void timecircuit_flicker() {
  unsigned long currentMillis = millis();

  // Check if the desired time has passed to update the LEDs
  if (currentMillis - previousMillis >= flickerSpeed) {
    previousMillis = currentMillis;  // Update the time for the next interval

    // Update all pixels with new random colors
    for (int i = 0; i < time_circuits.numPixels(); i++) {
      time_circuits.setPixelColor(i, time_circuits.Color(random(256), random(256), random(256)));
    }
    time_circuits.show();  // Update the LEDs with the new colors
  }
}


void console_flikr() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= fliker) {
    previousMillis = currentMillis;
    for (int i = 11; i <= 14; i++) {
      if (random(10) == 0) {
        machine.setPixelColor(i, 255, 255, 255);

      } else {
        machine.setPixelColor(i, 0, 0, 0);
      }
    }
    machine.show();
  }
}
void shocking() {
  unsigned long currentMillis = millis();
  if (machine_tracks[7]) {      // Note: Track 21 is at index 20 in a zero-based array
    mp3_machine.play(21);       // electric
    machine_tracks[7] = false;  // Set to false to prevent re-playing
  }
  if (currentMillis - previousMillis >= electric) {
    previousMillis = currentMillis;
    for (int i = 0; i < 20; i++) {
      machine.setPixelColor(i, 255, 255, 255);
    }

    delay(50);
    machine.clear();
    machine.show();

    if (currentMillis - previousMillis >= electric) {
      previousMillis = currentMillis;
      for (int i = 0; i < 20; i++) {
        time_display.setPixelColor(i, 255, 255, 255);
      }
      for (int i = 1; i < 50; i++) {
        machine.setPixelColor(i, 255, 255, 255);
      }
      time_display.show();
      machine.show();
      delay(50);
      time_display.clear();
      machine.clear();
      time_display.show();
      machine.show();
    }
  }
}
void twink() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= twinkle) {
    previousMillis = currentMillis;

    // Generate a new random interval for the next update
    twinkle = random(2, 100);  // Flash quicker with shorter intervals

    for (int i = 22; i < 28; i++) {
      if (random(97) == 0) {  // 50% chance for this LED to light up
        // Generate a random brightness level
        int brightness = random(5, 100);  // Adjust range for subtle or bright twinkle
        // Set the pixel with the random brightness (white light)
        machine.setPixelColor(i, brightness, brightness, brightness);
      } else {
        // Keep the pixel off
        machine.setPixelColor(i, 0, 0, 0);
      }
    }

    machine.show();  // Refresh the LEDs to show the changes
  }
}


void twentyfive() {

  //5
  // time_display.setPixelColor(0, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(1, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(2, 255, 0, 0);  // Set the pixel color
  // time_display.setPixelColor(3, 255, 0, 0); // Set the pixel color
  time_display.setPixelColor(4, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(5, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(6, 255, 0, 0);  // Set the pixel color

  /*------------------------------------------------------------*/
  //2

  time_display.setPixelColor(7, 255, 0, 0);   // Set the pixel color
  time_display.setPixelColor(8, 255, 0, 0);   // Set the pixel color
                                              // time_display.setPixelColor(9, 255, 0, 0); // Set the pixel color
  time_display.setPixelColor(10, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(11, 255, 0, 0);  // Set the pixel color
                                              // time_display.setPixelColor(12, 255, 0, 0); // Set the pixel color
  time_display.setPixelColor(13, 255, 0, 0);  // Set the pixel color

  /*------------------------------------------------------------*/
  //0

  time_display.setPixelColor(14, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(15, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(16, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(17, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(18, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(19, 255, 0, 0);  // Set the pixel color
                                              //  time_display.setPixelColor(20, 255, 0, 0); // Set the pixel color

  /*------------------------------------------------------------*/
  //2

  time_display.setPixelColor(21, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(22, 255, 0, 0);
  //time_display.setPixelColor(23, 255, 0, 0); // Set the pixel color
  time_display.setPixelColor(24, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(25, 255, 0, 0);  // Set the pixel color
                                              // time_display.setPixelColor(26, 255, 0, 0); // Set the pixel color
  time_display.setPixelColor(27, 255, 0, 0);  // Set the pixel color

  time_display.show();
}


void five_ten() {

  time_display.setPixelColor(0, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(1, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(2, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(3, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(4, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(5, 255, 0, 0);  // Set the pixel color
  // time_display.setPixelColor(6, 255, 0, 0); // Set the pixel color

  /*------------------------------------------------------------*/


  time_display.setPixelColor(7, 255, 0, 0);  // Set the pixel color
  // time_display.setPixelColor(8, 255, 0, 0); // Set the pixel color
  // time_display.setPixelColor(9, 255, 0, 0); // Set the pixel color
  //  time_display.setPixelColor(10, 255, 0, 0); // Set the pixel color
  // time_display.setPixelColor(11, 255, 0, 0); // Set the pixel color
  time_display.setPixelColor(12, 255, 0, 0);  // Set the pixel color
  // time_display.setPixelColor(13, 255, 0, 0); // Set the pixel color

  /*------------------------------------------------------------*/


  time_display.setPixelColor(14, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(15, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(16, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(17, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(18, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(19, 255, 0, 0);  // Set the pixel color
  // time_display.setPixelColor(20, 255, 0, 0); // Set the pixel color

  /*------------------------------------------------------------*/
  //5

  //time_display.setPixelColor(21, 255, 0, 0); // Set the pixel color
  time_display.setPixelColor(22, 255, 0, 0);
  time_display.setPixelColor(23, 255, 0, 0);  // Set the pixel color
  // time_display.setPixelColor(24, 255, 0, 0); // Set the pixel color
  time_display.setPixelColor(25, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(26, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(27, 255, 0, 0);  // Set the pixel color

  time_display.show();
}


void zero() {

  time_display.setPixelColor(0, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(1, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(2, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(3, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(4, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(5, 255, 0, 0);  // Set the pixel color
  //time_display.setPixelColor(6, 255, 0, 0); // Set the pixel color

  /*------------------------------------------------------------*/


  time_display.setPixelColor(7, 255, 0, 0);   // Set the pixel color
  time_display.setPixelColor(8, 255, 0, 0);   // Set the pixel color
  time_display.setPixelColor(9, 255, 0, 0);   // Set the pixel color
  time_display.setPixelColor(10, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(11, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(12, 255, 0, 0);  // Set the pixel color
  //  time_display.setPixelColor(13, 255, 0, 0); // Set the pixel color

  /*------------------------------------------------------------*/


  time_display.setPixelColor(14, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(15, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(16, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(17, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(18, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(19, 255, 0, 0);  // Set the pixel color
  //  time_display.setPixelColor(20, 255, 0, 0); // Set the pixel color

  /*------------------------------------------------------------*/


  time_display.setPixelColor(21, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(22, 255, 0, 0);
  time_display.setPixelColor(23, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(24, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(25, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(26, 255, 0, 0);  // Set the pixel color
  //  time_display.setPixelColor(27, 255, 0, 0); // Set the pixel color

  time_display.show();
}

void ninefourtwo() {  //

  time_display.setPixelColor(0, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(1, 255, 0, 0);  // Set the pixel color
  // time_display.setPixelColor(2, 255, 0, 0); // Set the pixel color
  time_display.setPixelColor(3, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(4, 255, 0, 0);  // Set the pixel color
  // time_display.setPixelColor(5, 255, 0, 0); // Set the pixel color
  time_display.setPixelColor(6, 255, 0, 0);  // Set the pixel color

  /*------------------------------------------------------------*/


  time_display.setPixelColor(7, 255, 0, 0);  // Set the pixel color
  // time_display.setPixelColor(8, 255, 0, 0); // Set the pixel color
  time_display.setPixelColor(9, 255, 0, 0);  // Set the pixel color
  //  time_display.setPixelColor(10, 255, 0, 0); // Set the pixel color
  // time_display.setPixelColor(11, 255, 0, 0); // Set the pixel color
  time_display.setPixelColor(12, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(13, 255, 0, 0);  // Set the pixel color

  /*------------------------------------------------------------*/


  time_display.setPixelColor(14, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(15, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(16, 255, 0, 0);  // Set the pixel color
  // time_display.setPixelColor(17, 255, 0, 0); // Set the pixel color
  time_display.setPixelColor(18, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(19, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(20, 255, 0, 0);  // Set the pixel color

  /*------------------------------------------------------------*/


  time_display.setPixelColor(21, 255, 0, 0);  // Set the pixel color
  // time_display.setPixelColor(22, 255, 0, 0);
  //  time_display.setPixelColor(23, 255, 0, 0); // Set the pixel color
  // time_display.setPixelColor(24, 255, 0, 0); // Set the pixel color
  // time_display.setPixelColor(25, 255, 0, 0); // Set the pixel color
  time_display.setPixelColor(26, 255, 0, 0);  // Set the pixel color
  //   time_display.setPixelColor(27, 255, 0, 0); // Set the pixel color

  time_display.show();
}

void oneeightfourtwo() {

  time_display.setPixelColor(0, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(1, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(2, 255, 0, 0);  // Set the pixel color
  // time_display.setPixelColor(3, 255, 0, 0); // Set the pixel color
  time_display.setPixelColor(4, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(5, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(6, 255, 0, 0);  // Set the pixel color

  /*------------------------------------------------------------*/


  time_display.setPixelColor(7, 255, 0, 0);   // Set the pixel color
  time_display.setPixelColor(8, 255, 0, 0);   // Set the pixel color
  time_display.setPixelColor(9, 255, 0, 0);   // Set the pixel color
  time_display.setPixelColor(10, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(11, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(12, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(13, 255, 0, 0);  // Set the pixel color

  /*------------------------------------------------------------*/


  time_display.setPixelColor(14, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(15, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(16, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(17, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(18, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(19, 255, 0, 0);  // Set the pixel color
  time_display.setPixelColor(20, 255, 0, 0);  // Set the pixel color

  /*------------------------------------------------------------*/


  time_display.setPixelColor(21, 255, 0, 0);  // Set the pixel color
  // time_display.setPixelColor(22, 255, 0, 0);
  //  time_display.setPixelColor(23, 255, 0, 0); // Set the pixel color
  // time_display.setPixelColor(24, 255, 0, 0); // Set the pixel color
  // time_display.setPixelColor(25, 255, 0, 0); // Set the pixel color
  time_display.setPixelColor(26, 255, 0, 0);  // Set the pixel color
  //   time_display.setPixelColor(27, 255, 0, 0); // Set the pixel color

  time_display.show();
}


void time_flikr() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= timefliker) {
    previousMillis = currentMillis;
    for (int i = 0; i <= 30; i++) {
      if (random(10) == 0) {
        time_display.setPixelColor(i, 255, 255, 255);

      } else {
        time_display.setPixelColor(i, 0, 0, 0);
      }
    }
    time_display.show();
  }
}

void console_fade() {
  unsigned long currentMillis = millis();

  // Fade only after a specified time has passed
  if (currentMillis - previousMillis >= fade_time) {
    previousMillis = currentMillis;

    // Smooth fade from black (0) to full brightness (255)
    for (int brightness = 0; brightness <= 255; brightness++) {  // Change direction of the loop
      for (int i = 0; i <= 9; i++) {
        // Set color with the current brightness level (RGB values for white)
        time_circuits.setPixelColor(i, time_circuits.Color(brightness, brightness, brightness));
      }

      time_circuits.show();  // Update LED colors only once after setting all pixels
      delay(10);             // Add a short delay for the fade effect to be visible
    }
  }
}
void console_fade_out() {
  unsigned long currentMillis = millis();

  // Fade only after a specified time has passed
  if (currentMillis - previousMillis >= fade_time) {
    previousMillis = currentMillis;

    // Smooth fade from full brightness (255) to black (0)
    for (int brightness = 255; brightness >= 0; brightness--) {
      for (int i = 11; i <= 17; i++) {
        // Set color with the current brightness level (RGB values for white)
        machine.setPixelColor(i, machine.Color(brightness, brightness, brightness));
      }

      machine.show();  // Update LED colors only once after setting all pixels
      delay(70);       // Add a short delay for the fade effect to be visible
    }
  }
}

// Function to handle pixel blinking
void updateBlinkingPixel(int pixel, int red, int green, int blue) {
  unsigned long currentMillis = millis();

  // Check if it's time to toggle the LED state
  if (currentMillis - previousMillis >= blink_interval) {
    previousMillis = currentMillis;  // Update the last time the state changed

    // Toggle the state
    isOn = !isOn;

    // Update the specified pixel
    if (isOn) {
      Display.setPixelColor(pixel, Display.Color(red, green, blue));  // Set to the specified color
    } else {
      Display.setPixelColor(pixel, 0);  // Turn off the pixel
    }

    // Update the display
    Display.show();
  }
}

void candles() {

  unsigned long currentMillis = millis();

  // Check if it's time to update the candle flicker
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    interval = random(30, 500);  // Update with a new random interval for flickering

    for (int i = 0; i < NUM_CANDLES; i++) {
      // Base color for a warm candle glow
      int redBase = random(180, 255);       // Base red value, higher for brightness
      int flickerOffset = random(-50, 50);  // Flicker variation
      int greenBase = redBase / 4;          // Smaller green value for a warm tone

      // Constrain values to ensure they stay within 0-255
      int red = constrain(redBase + flickerOffset, 0, 255);
      int green = constrain(greenBase + (flickerOffset / 2), 0, 255);

      // Set each LED in the candle to flicker
      for (int j = 0; j < NUM_PIXELS_PER_CANDLE; j++) {
        int pixelIndex = i * NUM_PIXELS_PER_CANDLE + j;
        pixels.setPixelColor(pixelIndex, pixels.Color(red, green, 0));
      }
    }
    pixels.show();  // Update the NeoPixels with the new colors
  }
}

const long twinkleDelay = 5;            // Delay in milliseconds for the twinkle effect
int ledIndices[] = { 18, 19, 20, 21 };  // Array of LED indices

void twinkleLEDs() {
  unsigned long currentMillis = millis();  // Get the current time

  // Check if it's time to update the LEDs
  if (currentMillis - previousMillis >= twinkleDelay) {
    previousMillis = currentMillis;  // Save the last update time

    // Update each LED randomly
    for (int i = 0; i < 4; i++) {
      int r = random(0, 2);  // Randomly turn the LED on (green) or off (black)
      if (r == 1) {
        machine.setPixelColor(ledIndices[i], 255, 255, 255);  // Turn LED green
      } else {
        machine.setPixelColor(ledIndices[i], 0, 0, 0);  // Turn LED off
      }
    }

    machine.show();  // Apply the changes to the LEDs
  }
}

// timing for scrolling text


const String message = "SHOWDUINO VER 0.1 - Toby Brandon  ";  // Message to scroll
const unsigned long scrollInterval = 200;                     // Scroll speed (milliseconds)
const unsigned long scrollDuration = 5000;                    // Total time to scroll (10 seconds)
int scrollPosition = 0;
// Variables for glitch effects
unsigned long previousMillisPixel1 = 0;
unsigned long previousMillisPixel2 = 0;
unsigned long glitchIntervalPixel1 = random(200, 700);  // Random initial interval for Pixel 1
unsigned long glitchIntervalPixel2 = random(200, 700);  // Random initial interval for Pixel 2

int brightnessPixel1 = 200;  // Initial brightness for Pixel 1
int brightnessPixel2 = 200;  // Initial brightness for Pixel 2

void glitchEffect() {
  unsigned long currentMillis = millis();

  // Handle Pixel 1 glitch
  if (currentMillis - previousMillisPixel1 >= glitchIntervalPixel1) {
    previousMillisPixel1 = currentMillis;
    glitchIntervalPixel1 = random(200, 700);  // Set a new random interval

    // Subtly vary the brightness for Pixel 1
    brightnessPixel1 = random(50, 150);                                     // Slight dimming or brightening
    machine.setPixelColor(1, brightnessPixel1, brightnessPixel1 * 0.8, 0);  // Dirty yellow
    machine.show();
  }

  // Handle Pixel 2 glitch
  if (currentMillis - previousMillisPixel2 >= glitchIntervalPixel2) {
    previousMillisPixel2 = currentMillis;
    glitchIntervalPixel2 = random(200, 700);  // Set a new random interval

    // Subtly vary the brightness for Pixel 2
    brightnessPixel2 = random(50, 150);                                     // Slight dimming or brightening
    machine.setPixelColor(2, brightnessPixel2, brightnessPixel2 * 0.8, 0);  // Dirty yellow
    machine.show();
  }
}

void glitchRed() {
  unsigned long currentMillis = millis();

  // Handle Pixel 1 glitch
  static unsigned long previousMillisPixel1 = 0;
  static int glitchIntervalPixel1 = random(200, 700);
  static int brightnessPixel1 = random(20, 255);

  if (currentMillis - previousMillisPixel1 >= glitchIntervalPixel1) {
    previousMillisPixel1 = currentMillis;
    glitchIntervalPixel1 = random(200, 700);  // Set a new random interval

    // Subtly vary the brightness for Pixel 1
    brightnessPixel1 = random(20, 255);                // Slight dimming or brightening
    machine.setPixelColor(1, brightnessPixel1, 0, 0);  // Pure red with varying brightness
    machine.show();
  }

  // Handle Pixel 2 glitch
  static unsigned long previousMillisPixel2 = 0;
  static int glitchIntervalPixel2 = random(200, 700);
  static int brightnessPixel2 = random(50, 150);

  if (currentMillis - previousMillisPixel2 >= glitchIntervalPixel2) {
    previousMillisPixel2 = currentMillis;
    glitchIntervalPixel2 = random(200, 700);  // Set a new random interval

    // Subtly vary the brightness for Pixel 2
    brightnessPixel2 = random(50, 255);                // Slight dimming or brightening
    machine.setPixelColor(2, brightnessPixel2, 0, 0);  // Pure red with varying brightness
    machine.show();
  }
}

/*
 buzzer sounds 
 S_CONNECTION   S_DISCONNECTION S_BUTTON_PUSHED   
 S_MODE1        S_MODE2         S_MODE3     
 S_SURPRISE     S_OHOOH         S_OHOOH2    
 S_CUDDLY       S_SLEEPING      S_HAPPY     
 S_SUPER_HAPPY  S_HAPPY_SHORT   S_SAD       
 S_CONFUSED     S_FART1         S_FART2     
 S_FART3        S_JUMP 20

 */


void setup() {
  // Initialize time tracking and hardware components

  lcd.init();  // initialize the lcd
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("SHOWDUINO- 1.892");
  lcd.setCursor(2, 1);

  lcd.print("STARTING ....");


  // Set RELAY outputs

  pinMode(relay1, OUTPUT);
  pinMode(relay2, OUTPUT);
  pinMode(relay3, OUTPUT);
  pinMode(relay4, OUTPUT);
  pinMode(relay5, OUTPUT);
  pinMode(relay6, OUTPUT);
  pinMode(relay7, OUTPUT);
  pinMode(relay8, OUTPUT);
  randomSeed(millis());  // Seed randomness with time

  digitalWrite(relay1, HIGH);
  digitalWrite(relay2, HIGH);
  digitalWrite(relay3, HIGH);
  digitalWrite(relay4, HIGH);
  digitalWrite(relay5, HIGH);
  digitalWrite(relay6, HIGH);
  digitalWrite(relay7, HIGH);
  digitalWrite(relay8, HIGH);

  // Initialize serial and MP3 communication
  Serial.begin(9600);

  mp3_ambient.begin(9600);
  delay(500);  // wait for init

  mp3_ambient.sendCommand(CMD_SEL_DEV, 0, 2);  // Select SD card for mp3_ambient
  mp3_ambient.setVol(30);
  mp3_ambient.stop();

  // Initialize the array elements to true
  for (int i = 0; i < 30; i++) {
    ambient_tracks[i] = true;  // Set each element to true

    mp3_machine.begin(9600);
    delay(500);  // wait for init

    mp3_machine.sendCommand(CMD_SEL_DEV, 0, 2);  // Select SD card for mp3_machine
    mp3_machine.setVol(30);
    mp3_machine.stop();
  }
  for (int i = 0; i < 30; i++) {
    ambient_tracks[i] = true;  // Set each element to true
  }

  // Initialize LED displays
  machine.begin();
  machine.clear();
  machine.show();
  time_display.begin();
  time_display.clear();
  time_display.show();
  time_circuits.begin();
  time_circuits.clear();
  time_circuits.show();
  pixels.begin();
  pixels.clear();
  pixels.show();
  spotlights.begin();
  Display.begin();

  cute.init(BUZZER_PIN);
  cute.play(S_CONNECTION);


  // Initialize pins for buttons and outputs
  pinMode(startbutton, INPUT_PULLUP);
  pinMode(emergency_stop, INPUT_PULLUP);
  pinMode(resetButton, INPUT_PULLUP);
  pinMode(SC1, INPUT_PULLUP);
  pinMode(SC2, INPUT_PULLUP);
  pinMode(SC3, INPUT_PULLUP);
  pinMode(SC4, INPUT_PULLUP);
  pinMode(SC5, INPUT_PULLUP);
  pinMode(SC6, INPUT_PULLUP);
  pinMode(SC7, INPUT_PULLUP);
  pinMode(SC8, INPUT_PULLUP);

  pinMode(ONESHOT1, INPUT_PULLUP);
  pinMode(ONESHOT2, INPUT_PULLUP);
  pinMode(ONESHOT3, INPUT_PULLUP);
  pinMode(ONESHOT4, INPUT_PULLUP);
  lcd.setCursor(0, 1);
  lcd.print("Setup running");

  // Scroll message across LCD until setup complete
  Serial.println("showduino - The arduino show controller");
  Display.clear();
  Display.setPixelColor(0, 0, 255, 0);  // Change color to green
  Display.show();

  // Final setup message and preparation
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("setup complete");
  delay(1000);
  lcd.setCursor(0, 0);
  lcd.print("select scene");
  cute.play(S_HAPPY_SHORT);
}














void loop() {
  // unsigned long currentMillis = millis();
  // unsigned long elapsedMillis = millis() - startMillis;
  bool startPressed = !digitalRead(startbutton);
  bool emergencyPressed = !digitalRead(emergency_stop);
  bool resetPressed = !digitalRead(resetButton);

  if (!oneshot1_on && !digitalRead(ONESHOT1)) {
  }
  if (!oneshot2_on && !digitalRead(ONESHOT2)) {
  }

  if (!oneshot3_on && !digitalRead(ONESHOT3)) {
  }

  if (!oneshot4_on && !digitalRead(ONESHOT4)) {
  }

  if (emergencyStopActive) {  // this will repeat until next action
    lcd.setCursor(0, 1);
    lcd.print("EMERGENCY STOP     ");
    mp3_ambient.stop();
    mp3_machine.stop();
    circuit_flick = false;
    machine.clear();
    for (int i = 0; i <= 100; i++) {
      machine.setPixelColor(i, machine.Color(255, 255, 255));  // Green color
    }
    machine.show();
    time_display.clear();
    for (int i = 0; i <= 100; i++) {
      time_display.setPixelColor(i, time_display.Color(255, 255, 255));  // Green color
    }
    time_display.show();
    pixels.clear();
    for (int i = 0; i <= 24; i++) {
      pixels.setPixelColor(i, pixels.Color(255, 255, 255));  // Green color
    }
    pixels.show();
    time_circuits.clear();
    for (int i = 0; i <= 100; i++) {
      time_circuits.setPixelColor(i, time_circuits.Color(255, 255, 255));  // Green color
    }
    time_circuits.show();
    Serial.println("Emergency Stop Activated");
  }

  if (isshocking) {
    shocking();
  }
  if (bombs) {
    twink();
  }
  if (scene10Active && istravelling) {
    Serial.println("Flicker functions executing...");
    time_flikr();
    timecircuit_flicker();
  }
  if (circuit_flick) {
    timecircuit_flicker();
    // time_flikr();
  }

  if (isglitching) {
    glitchRed();
  }

  // Emergency stop logic
  if (emergencyPressed && !emergencyStopActive) {
    emergencyStopActive = true;
    isshocking = false;
    // circuit_flick = false;
    bombs = false;
    mp3_ambient.stop();
    mp3_machine.stop();
    lcd.setCursor(0, 1);
    lcd.print("MACHINE: stopped");
    scene1 = false;
    scene2 = false;
    scene3 = false;
    scene4 = false;
    scene5 = false;
    scene6 = false;
    scene7 = false;
    scene8 = false;
    scene9 = false;
    scene10 = false;
    istravelling = false;
    isglitching = false;
    scene2_1 = false;
    scene6_1 = false;
    flash1Printed = false;
    flash2Printed = false;
    flash3Printed = false;
    flash4Printed = false;
    flash5Printed = false;
    flash6Printed = false;
    flash7Printed = false;
    flash8Printed = false;
    flash9Printed = false;
    flash10Printed = false;
    flash11Printed = false;
    flash12Printed = false;
    Printed1 = false;
    Printed2 = false;
    Printed3 = false;
    Printed4 = false;
    Printed5 = false;
    Printed6 = false;
    Printed7 = false;
    Printed8 = false;
    Printed9 = false;



    digitalWrite(relay1, HIGH);
    digitalWrite(relay2, HIGH);
    digitalWrite(relay3, HIGH);
    digitalWrite(relay4, HIGH);
    digitalWrite(relay5, HIGH);
    digitalWrite(relay6, HIGH);
    digitalWrite(relay7, HIGH);
    digitalWrite(relay8, HIGH);

    Display.clear();
    Display.show();
  }

  // Reset button logic
  if (resetPressed && emergencyStopActive) {
    emergencyStopActive = false;
    // circuit_flick = false;
    Serial.println("System Reset");
    lcd.setCursor(0, 1);
    lcd.print("System Reset     ");
    delay(1000);
    lcd.setCursor(0, 1);
    lcd.print("start/scene select   ");
    updateBlinkingPixel(1, 0, 0, 255);  // Blink pixel 1 with blue color
    machine.clear();
    machine.show();
    time_display.clear();
    time_display.show();
    pixels.clear();
    pixels.show();
    time_circuits.clear();
    time_circuits.show();
    Display.clear();
    Display.show();
    machineStarted = false;
    scene1 = false;
    scene2 = false;
    scene3 = false;
    scene4 = false;
    scene5 = false;
    scene6 = false;
    scene7 = false;
    scene8 = false;
    scene9 = false;
    scene10 = false;
    scene1Active = false;
    scene2Active = false;
    scene3Active = false;
    scene4Active = false;
    scene5Active = false;
    scene6Active = false;
    scene7Active = false;
    scene8Active = false;
    scene9Active = false;
    scene10Active = false;
    scene2_1 = false;
    scene6_1 = false;
    isshocking = false;
    bombs = false;
    circuit_flick = false;
    istravelling = false;
    isglitching = false;
    waiting = false;
    oneshot1_on = false;
    oneshot2_on = false;
    oneshot3_on = false;
    oneshot4_on = false;
    waitingActive = false;
    scene3Waiting = false;
    scene4Waiting = false;
    scene5Waiting = false;
    scene6Waiting = false;
    scene7Waiting = false;
    scene8Waiting = false;
    scene9Waiting = false;
    scene10Waiting = false;
    // Initialize the array elements to true
    for (int i = 0; i < 30; i++) {
      ambient_tracks[i] = true;  // Set each element to true
    }
    for (int i = 0; i < 30; i++) {
      machine_tracks[i] = true;  // Set each element to true
    }
    lcd.setCursor(0, 0);
    lcd.print("MACHINE: stopped");
  }



  // Start button logic
  if (startPressed && !machineStarted && !emergencyStopActive) {
    machineStarted = true;
    scene1Active = true;
    for (int i = 0; i < 30; i++) {
      ambient_tracks[i] = true;  // Set each element to true
    }
    for (int i = 0; i < 30; i++) {
      machine_tracks[i] = true;  // Set each element to true
    }
    mp3_ambient.setVol(30);
    mp3_machine.setVol(30);
    Display.setPixelColor(3, 0, 255, 0);  // Change color to green
    Display.show();
    if (ambient_tracks[20]) {  // Note: Track 21 is at index 20 in a zero-based array
      mp3_ambient.play(21);
      console_fade();
      ambient_tracks[20] = false;  // Set to false to prevent re-playing
    }
    candles();
    Serial.println("console fade in");
    lcd.setCursor(0, 1);
    lcd.print("show starting  ");
    Serial.println("Machine Started");
    lcd.setCursor(0, 0);
    lcd.print("MACHINE: running");
  }


  // Check if machine is running
  if (machineStarted && !emergencyStopActive) {
    unsigned long currentMillis = millis();
    unsigned long elapsedMillis = currentMillis - startMillis;
    startMillis = millis();  // Reset the timer
    candles();
    console_flikr();
    scene1Active = false;
    // Scene 1:    lights on
    if (elapsedMillis >= 18000 && !scene1) {
      scene1 = true;
      twentyfive();
      circuit_flick = true;
      timecircuit_flicker();
      console_flikr();
      for (int i = 29; i < 42; i++) {
        machine.setPixelColor(i, 255, 255, 255);
      }
      machine.setPixelColor(18, 0, 255, 0);  // Set the pixel color      // door open/green
      machine.setPixelColor(19, 0, 255, 0);  // Set the pixel color
      machine.setPixelColor(20, 0, 255, 0);  // Set the pixel color
      machine.setPixelColor(21, 0, 255, 0);  // Set the pixel color     // door open/green
      mp3_ambient.setVol(30);
      Serial.println("Scene 1 Activated - machine start");
      machine.setPixelColor(1, 255, 255, 0);    // Set the pixel color
      machine.setPixelColor(2, 255, 255, 255);  // Set the pixel color
      for (int i = 3; i <= 10; i++) {
        machine.setPixelColor(i, machine.Color(0, 0, 0));  // zero color
      }
      for (int i = 22; i < 28; i++) {
        machine.setPixelColor(i, 255, 255, 255);
      }
      machine.show();
      lcd.setCursor(0, 1);
      lcd.print("Scene 1 Active");
      //startMillis = millis();               // Update the start time for scene 2
    }
    // Scene 2:     steam on
    if (elapsedMillis >= 23000 && !scene2) {
      scene2Active = true;
      scene1 = false;
      scene2 = true;
      Display.setPixelColor(3, 0, 0, 0);    // Change color to green
      Display.setPixelColor(4, 0, 255, 0);  // Change color to green
      Display.show();
      timecircuit_flicker();

      if (machine_tracks[24]) {      // Note: Tracks is in a zero-based array
        mp3_machine.play(24);        // steam sound
        machine_tracks[24] = false;  // Set to false to prevent re-playing
      }
      for (int i = 3; i <= 10; i++) {
        machine.setPixelColor(i, machine.Color(0, 255, 0));  // Green color
      }
      Serial.println("Scene 2 Activated");
      for (int i = 29; i < 42; i++) {
        machine.setPixelColor(i, 255, 255, 255);
      }
      machine.setPixelColor(0, 255, 255, 255);  // Set the pixel color
      machine.show();
      lcd.setCursor(0, 1);
      lcd.print("Scene 2 Active");
    }
    // Scene 2-1:   steam off - radio 1
    if (elapsedMillis >= 26000 && !scene2_1) {
      scene2Active = false;
      scene2 = false;
      scene2_1 = true;
      Serial.println("Scene 2-1 Activated");
      machine_tracks[16] = true;
      timecircuit_flicker();
      if (machine_tracks[16]) {      // Note: Tracks is in a zero-based array
        mp3_machine.play(16);        // radio 1 sound
        machine_tracks[16] = false;  // Set to false to prevent re-playing
      }

      for (int i = 29; i < 42; i++) {
        machine.setPixelColor(i, 255, 255, 255);
      }
      Display.setPixelColor(4, 0, 0, 0);    // Change color to green
      Display.setPixelColor(5, 0, 255, 0);  // Change color to green
      Display.show();
      lcd.setCursor(0, 1);
      lcd.print("Scene 2-1 Active      ");
      machine.setPixelColor(0, 0, 0, 0);  // Set the pixel color
      machine.show();
    }
    // Scene 3: radio 2 INTO WAITING
    if (elapsedMillis >= 29000 && !scene3) {
      scene2_1active = true;
      scene3 = true;
      scene2_1 = false;
      timecircuit_flicker();
      machine_tracks[15] = true;
      if (machine_tracks[15]) {      // Note: Tracks is in a zero-based array
        mp3_machine.play(15);        // radio 2 sound
        machine_tracks[15] = false;  // Set to false to prevent re-playing
      }
      Display.setPixelColor(5, 0, 0, 0);    // Change color to green
      Display.setPixelColor(6, 0, 255, 0);  // Change color to green
      Display.show();
      Serial.println("Scene 3 Activated");
      lcd.setCursor(0, 1);
      lcd.print("Scene 3 Active       ");
      for (int i = 11; i <= 17; i++) {
        machine.setPixelColor(i, machine.Color(0, 255, 0));  // Green color
      }
      for (int i = 29; i < 42; i++) {
        machine.setPixelColor(i, 255, 255, 255);
      }
      machine.show();



      /************************************ Start the waiting period for Scene 3  *****************************************/

      scene3Waiting = true;
      startMillis = millis();  // Reset the timer for the waiting period
    }



    // Scene 3 waiting period: radio 3
    if (scene3Waiting && (millis() - startMillis >= 5000)) {
      timecircuit_flicker();
      machine_tracks[14] = true;
      if (machine_tracks[14]) {      // Note: Tracks is in a zero-based array
        mp3_machine.play(14);        // radio 3 sound
        machine_tracks[14] = false;  // Set to false to prevent re-playing
      }

      scene3Waiting = false;
      scene3 = false;
      scene2_1 = false;
      scene2 = false;
      scene1 = false;
      delay(8000);
      mp3_machine.setVol(30);
      machine_tracks[26] = true;
      if (machine_tracks[26]) {      // Note: Tracks is in a zero-based array
        mp3_machine.play(26);        // blast shield sound
        machine_tracks[26] = false;  // Set to false to prevent re-playing
      }
      machine.setPixelColor(18, 0, 0, 255);  // Set the pixel color      // door locked
      machine.setPixelColor(19, 0, 0, 255);  // Set the pixel color
      machine.setPixelColor(20, 0, 0, 255);  // Set the pixel color
      machine.setPixelColor(21, 0, 0, 255);  // Set the pixel color     // door locked
      for (int i = 29; i < 42; i++) {
        machine.setPixelColor(i, 255, 255, 255);
      }
      machine.show();
      lcd.setCursor(0, 0);
      lcd.print("sc3 waiting      ");
      Serial.println("Scene 3 waiting");
      flickerSpeed = 500;
      Display.setPixelColor(6, 0, 0, 255);  // Change color to green
      Display.show();
      machineStarted = false;
      updateBlinkingPixel(1, 0, 0, 255);  // Blink pixel 1 with blue color
    }
  }


  // Scene 4: Activate with SC1 button
  if (!scene4Active && !digitalRead(SC1)) {
    startMillis = millis();               // Update the start time for scene 5
    Display.setPixelColor(6, 0, 0, 0);    // Change color to green
    Display.setPixelColor(7, 0, 255, 0);  // Change color to green
    Display.show();
    scene1 = false;
    scene2 = false;
    scene3 = false;
    timecircuit_flicker();
    machine.setPixelColor(18, 0, 0, 255);  // Set the pixel color      // door locked
    machine.setPixelColor(19, 0, 0, 255);  // Set the pixel color
    machine.setPixelColor(20, 0, 0, 255);  // Set the pixel color
    machine.setPixelColor(21, 0, 0, 255);  // Set the pixel color     // door locked
    machine.show();
    istravelling = true;
    mp3_machine.setVol(30);
    // digitalWrite(relay1, LOW);  // Turn the RELAY Off
    timecircuit_flicker();
    scene4Active = true;
    Serial.println("Scene 4 Activated by SC1");
    flickerSpeed = 10;
    lcd.setCursor(0, 0);
    lcd.print("MACHINE: running");
    lcd.setCursor(0, 1);
    lcd.print("sc4 travelling");
    bombs = false;
    flickerSpeed = 10;
    time_flikr();
    timecircuit_flicker();
  }

  // Running Scene 4 "travel to dino"
  if (scene4Active) {
    unsigned long scene4ElapsedTime = millis() - startMillis;
    timecircuit_flicker();
    for (int i = 29; i < 42; i++) {
      machine.setPixelColor(i, 255, 255, 255);
    }
    machine.show();
    if (ambient_tracks[7]) {      // Note: Track 21 is at index 20 in a zero-based array
      mp3_ambient.play(8);        // travelling sound this has to be played in machine sound( not ambient)
      ambient_tracks[7] = false;  // Set to false to prevent re-playing
    }
    if (scene4ElapsedTime >= 0) {
      lcd.setCursor(0, 1);
      lcd.print("travel to dino      ");
      Serial.println("travel to dino");
      timecircuit_flicker();
      time_flikr();
      console_flikr();
      for (int i = 11; i <= 17; i++) {
        machine.setPixelColor(i, machine.Color(0, 0, 0));  // Green color
      }
      machine.setPixelColor(1, 255, 255, 0);    // Set the pixel color
      machine.setPixelColor(2, 255, 255, 255);  // Set the pixel color
      machine.show();
      flickerSpeed = 10;
    }

    // End Scene 4 after 5 seconds
    if (scene4ElapsedTime >= 30000) {
      scene4Active = false;
      time_display.clear();
      zero();
      digitalWrite(relay1, LOW);
      flickerSpeed = 500;
      lcd.setCursor(0, 1);
      lcd.print("sc4 dino scene");
      for (int i = 1; i <= 2; i++) {
        machine.setPixelColor(i, machine.Color(0, 255, 0));  // Green color
      }
      for (int i = 11; i <= 17; i++) {
        machine.setPixelColor(i, machine.Color(0, 255, 0));  // green color
      }
      Serial.println("dino scene");
      for (int i = 22; i < 28; i++) {
        machine.setPixelColor(i, 0, 50, 0);
      }
      machine.show();
      delay(4000);                  // dirty wait so mp3 tracks can layer
      if (ambient_tracks[5]) {      // Note: Track 21 is at index 20 in a zero-based array
        mp3_ambient.play(6);        // rainforest/dino
        ambient_tracks[5] = false;  // Set to false to prevent re-playing
      }
      scene4 = true;
      Serial.println("Scene 4 Complete");
      mp3_machine.play(13);  // radio year 0000
      for (int i = 29; i < 42; i++) {
        machine.setPixelColor(i, 255, 255, 255);
      }
      machine.show();
      delay(7000);
      startMillis = millis();  // Reset the timer for Scene 4 waiting period
      scene4Waiting = true;
    }
  }

  // Scene 4 waiting period after Scene 4 ends
  if (scene4Waiting && (millis() - startMillis >= 1000)) {  // Adjust waiting duration as needed
    machine_tracks[26] = true;
    scene4Waiting = false;
    scene4Active = false;  // Deactivate Scene 4
    mp3_machine.play(12);  // life forces)
    Serial.println("dino waiting scene");
    delay(10000);
    if (machine_tracks[26]) {      // Note: Tracks is in a zero-based array
      mp3_machine.play(26);        // blast shield sound
      machine_tracks[26] = false;  // Set to false to prevent re-playing
    }
    machine.setPixelColor(18, 0, 255, 0);  // Set the pixel color      // door green
    machine.setPixelColor(19, 0, 255, 0);  // Set the pixel color
    machine.setPixelColor(20, 0, 255, 0);  // Set the pixel color
    machine.setPixelColor(21, 0, 255, 0);  // Set the pixel color     // door green
    machine.show();
    lcd.setCursor(0, 0);
    lcd.print("Sc4 waiting        ");
    Serial.println("Sc4 waiting");
    delay(8000);
    mp3_machine.play(11);  // hold tight)
    machine_tracks[26] = true;
    Display.setPixelColor(7, 0, 0, 255);  // Change color to green
    Display.show();
  }

  // Scene 5: Activate with SC2 button
  if (!scene5Active && !digitalRead(SC2)) {
    startMillis = millis();               // Update the start time for scene 5
    Display.setPixelColor(7, 0, 0, 0);    // Change color to green
    Display.setPixelColor(8, 0, 255, 0);  // Change color to green
    Display.show();
    if (machine_tracks[26]) {      // Note: Tracks is in a zero-based array
      mp3_machine.play(26);        // blast shield sound
      machine_tracks[26] = false;  // Set to false to prevent re-playing
    }
    machine.setPixelColor(18, 0, 255, 0);  // Set the pixel color      // door green
    machine.setPixelColor(19, 0, 255, 0);  // Set the pixel color
    machine.setPixelColor(20, 0, 255, 0);  // Set the pixel color
    machine.setPixelColor(21, 0, 255, 0);  // Set the pixel color     // door green
    machine.show();
    scene4Active = false;
    scene6Active = false;
    for (int i = 29; i < 42; i++) {
      machine.setPixelColor(i, 255, 255, 255);
    }
    machine.show();
    bombs = false;
    circuit_flick = true;
    // digitalWrite(relay1, LOW);  // Turn the RELAY Off
    scene5Active = true;
    for (int i = 11; i <= 17; i++) {
      machine.setPixelColor(i, machine.Color(0, 255, 0));  // green color
    }
    Serial.println("Scene 5 Activated by SC2");
    lcd.setCursor(0, 1);
    lcd.print("sc5 travelling ");
    ambient_tracks[7] = true;
    for (int i = 11; i <= 17; i++) {
      machine.setPixelColor(i, machine.Color(0, 0, 0));  // zero color
    }
    machine.show();
    machine.setPixelColor(1, 255, 255, 0);    // Set the pixel color
    machine.setPixelColor(2, 255, 255, 255);  // Set the pixel color
    machine.setPixelColor(18, 0, 0, 255);     // Set the pixel color      // door blue
    machine.setPixelColor(19, 0, 0, 255);     // Set the pixel color
    machine.setPixelColor(20, 0, 0, 255);     // Set the pixel color
    machine.setPixelColor(21, 0, 0, 255);     // Set the pixel color     // door blue
    machine.show();
    lcd.setCursor(0, 0);
    lcd.print("MACHINE: running");
    flickerSpeed = 10;
    time_flikr();
    console_flikr();
  }

  // Running Scene 5 water
  if (scene5Active) {
    unsigned long scene5ElapsedTime = millis() - startMillis;
    digitalWrite(relay1, HIGH);
    if (ambient_tracks[7]) {      // Note: Track 21 is at index 20 in a zero-based array
      mp3_ambient.play(8);        // travelling sound
      ambient_tracks[7] = false;  // Set to false to prevent re-playing
    }
    if (scene5ElapsedTime >= 0) {
      lcd.setCursor(0, 1);
      lcd.print("travel to water");
      flickerSpeed = 10;
      time_flikr();
      timecircuit_flicker();
    }

    // End Scene 5 after a duration
    if (scene5ElapsedTime >= 29500) {
      // Track 22 is at index 21 in a zero-based array
      machine.setPixelColor(18, 0, 0, 255);  // Set the pixel color      // door blue
      machine.setPixelColor(19, 0, 0, 255);  // Set the pixel color
      machine.setPixelColor(20, 0, 0, 255);  // Set the pixel color
      machine.setPixelColor(21, 0, 0, 255);  // Set the pixel color     // door blue
      machine.show();
      scene5Active = false;  // Deactivate the scene
      Serial.println("Scene 5 Complete");
      istravelling = false;
      flickerSpeed = 2000;
      time_display.clear();
      five_ten();
      for (int i = 22; i < 28; i++) {
        machine.setPixelColor(i, 0, 0, 255);
      }
      machine.show();
      ambient_tracks[7] = true;
      lcd.setCursor(0, 1);
      lcd.print("sc5 water scene ");
      for (int i = 1; i <= 2; i++) {
        machine.setPixelColor(i, machine.Color(0, 0, 255));  // blue color
      }
      for (int i = 11; i <= 17; i++) {
        machine.setPixelColor(i, machine.Color(0, 0, 255));  // blue color
      }
      machine.show();

      if (scene5ElapsedTime >= 29500) {

        scene5Waiting = true;
        startMillis = millis();  // Reset the timer for Scene 4 waiting period
        delay(3000);
        mp3_ambient.play(23);  // Play track 22 ("whales")
        delay(3000);
        mp3_machine.play(10);
      }
    }
  }


  // Scene 5 waiting period
  if (scene5Waiting && (millis() - startMillis >= 5000)) {  // Adjust waiting duration as needed
    scene5Waiting = false;
    Display.setPixelColor(8, 0, 0, 255);  // Change color to blue
    Display.show();
    lcd.setCursor(0, 0);
    lcd.print("sc5 waiting     ");
  }

  // Scene 6: Activate with SC3 button
  if (!scene6Active && !digitalRead(SC3)) {
    startMillis = millis();               // Update the start time for scene 5
    Display.setPixelColor(8, 0, 0, 0);    // Change color to zero
    Display.setPixelColor(9, 0, 255, 0);  // Change color to green
    Display.show();
    scene7Active = false;
    scene5Active = false;
    scene6Active = true;
    machine.clear();
    machine.show();
    Serial.println("Scene 6 Activated by SC3");
    lcd.setCursor(0, 1);
    lcd.print("sc6 travelling ");
    bombs = false;
    lcd.setCursor(0, 0);
    lcd.print("MACHINE: running");
    flickerSpeed = 10;
    time_flikr();
    console_flikr();
    machine.setPixelColor(1, 255, 255, 0);    // Set the pixel color
    machine.setPixelColor(2, 255, 255, 255);  // Set the pixel color
    machine.show();  }

  // Running Scene 6 war
  if (scene6Active) {
    unsigned long Scene6ElapsedTime = millis() - startMillis;
    if (ambient_tracks[7]) {      // Note: Track 21 is at index 20 in a zero-based array
      mp3_ambient.play(8);        // travelling sound
      ambient_tracks[7] = false;  // Set to false to prevent re-playing
    }
    if (Scene6ElapsedTime >= 0) {
      lcd.setCursor(0, 1);
      lcd.print("travel to war    ");
      flickerSpeed = 10;
      time_flikr();
      console_flikr();
    }
    if (Scene6ElapsedTime >= 100) {
      twinkleLEDs();
    }
    if (Scene6ElapsedTime >= 2000) {
      machine.setPixelColor(18, 255, 0, 0);  // Set the pixel color      // door locked
      machine.setPixelColor(19, 255, 0, 0);  // Set the pixel color
      machine.setPixelColor(20, 255, 0, 0);  // Set the pixel color
      machine.setPixelColor(21, 255, 0, 0);  // Set the pixel color     // door locked
      machine.show();
    }
    if (Scene6ElapsedTime >= 3000) {
      if (machine_tracks[9]) {      // Note: Track 21 is at index 20 in a zero-based array
        mp3_machine.play(9);        // lock fail
        machine_tracks[9] = false;  // Set to false to prevent re-playing
      }
      // machine.show();
    }
    if (Scene6ElapsedTime >= 8000) {
      if (machine_tracks[4]) {      // Note: Track 21 is at index 20 in a zero-based array
        mp3_machine.play(8);        // losing power
        machine_tracks[4] = false;  // Set to false to prevent re-playing
      }
      if (Scene6ElapsedTime >= 15000) {
        lcd.setCursor(0, 1);
        lcd.print("travel to war    ");
        circuit_flick = false;
        time_circuits.clear();
        time_circuits.show();
      }
      if (Scene6ElapsedTime >= 15500) {

        // room for more?
      }
      if (Scene6ElapsedTime >= 16000) {

        if (ambient_tracks[11]) {      // Note: Track 21 is at index 20 in a zero-based array
          mp3_ambient.play(4);         // warning sound
          ambient_tracks[11] = false;  // Set to false to prevent re-playing
        }
        machine.setPixelColor(1, 255, 0, 0);  // Set the pixel color      // door locked
        machine.setPixelColor(2, 255, 0, 0);  // Set the pixel color      // door locked
        for (int i = 22; i < 28; i++) {
          machine.setPixelColor(i, 255, 0, 0);
        }
        machine.show();
        for (int i = 3; i <= 10; i++) {
          machine.setPixelColor(i, machine.Color(255, 0, 0));  // red color
          machine.show();
        }
        isshocking = true;
        scene6_1 = true;
        circuit_flick = true;
        lcd.setCursor(0, 1);
        lcd.print("shock scene        ");
        Serial.println("SHOCK SCENE");
        time_display.clear();
      }

      // End Scene 6 after a duration
      if (Scene6ElapsedTime >= 23000) {
        mp3_machine.play(17);
        isshocking = false;
        circuit_flick = true;
        scene6Active = false;  // Deactivate the scene
        scene6_1 = false;
        Serial.println("Scene 6 Complete");
        time_circuits.clear();
        time_circuits.show();
        flickerSpeed = 2000;
        time_display.clear();
        ninefourtwo();
        machine.clear();
        machine.show();
        bombs = true;
        if (ambient_tracks[2]) {      // Note: Track is at index in a zero-based array
          mp3_ambient.play(3);        // GUNS
          ambient_tracks[2] = false;  // Set to false to prevent re-playing
        }
        scene6 = true;
        isglitching = true;
        lcd.setCursor(0, 1);
        lcd.print("sc6 war scene        ");
        machine_tracks[24] = true;
        delay(5000);
        machine.setPixelColor(18, 0, 255, 0);  // Set the pixel color      // door green
        machine.setPixelColor(19, 0, 255, 0);  // Set the pixel color
        machine.setPixelColor(20, 0, 255, 0);  // Set the pixel color
        machine.setPixelColor(21, 0, 255, 0);  // Set the pixel color     // door green
        machine.show();
        if (machine_tracks[24]) {      // Note: Track 21 is at index 20 in a zero-based array
          mp3_machine.play(24);        // door
          machine_tracks[24] = false;  // Set to false to prevent re-playing
        }
        //Start the waiting period for Scene 6
        scene6Waiting = true;
        machine_tracks[4] = true;
        ambient_tracks[7] = true;
        startMillis = millis();  // Reset the timer for Scene 6 waiting period
      }




      // Scene 6 waiting period
      if (scene6Waiting && (millis() - startMillis >= 5000)) {  // Adjust waiting duration as needed
        scene6Waiting = false;
        lcd.setCursor(0, 0);
        lcd.print("sc6 waiting     ");
        Display.setPixelColor(9, 0, 0, 255);  // Change color to green
        Display.show();
      }
    }
  }


  // Scene 7: Activate with SC4 button
  if (!scene7Active && !digitalRead(SC4)) {
    startMillis = millis();                // Update the start time for scene 7
    Display.setPixelColor(9, 0, 0, 0);     // Change color to zero
    Display.setPixelColor(10, 0, 255, 0);  // Change color to green
    Display.show();
    isglitching = true;
    scene8Active = false;
    machine_tracks[24] = true;
    scene7Active = true;
    Serial.println("Scene 7 Activated by SC4");
    candles();
    circuit_flick = false;
    flickerSpeed = 10;
    time_flikr();
    machine.clear();
    machine.show();
    lcd.setCursor(0, 0);
    lcd.print("MACHINE: running");
    lcd.setCursor(0, 1);
    lcd.print("sc7 travelling      ");
  }


  // Running Scene 7 with multiple steps
  if (scene7Active) {
    candles();
    unsigned long scene7ElapsedTime = millis() - startMillis;
    machine.setPixelColor(18, 255, 0, 0);  // Set the pixel color      // door locked
    machine.setPixelColor(19, 255, 0, 0);  // Set the pixel color
    machine.setPixelColor(20, 255, 0, 0);  // Set the pixel color
    machine.setPixelColor(21, 255, 0, 0);  // Set the pixel color     // door locked
    for (int i = 3; i <= 10; i++) {
      machine.setPixelColor(i, machine.Color(255, 0, 0));  // red color
      machine.show();
    }
    if (machine_tracks[24]) {      // Note: Track 21 is at index 20 in a zero-based array
      mp3_machine.play(24);        // door
      machine_tracks[24] = false;  // Set to false to prevent re-playing
    }

    machine.show();
    if (ambient_tracks[7]) {      // Note: Track 21 is at index 20 in a zero-based array
      mp3_ambient.play(8);        // travelling sound
      ambient_tracks[7] = false;  // Set to false to prevent re-playing
    }
    // Step 1: Flicker for the first 5 seconds
    if (scene7ElapsedTime < 10000) {
      // candles();
      time_flikr();
    }

    // Step 2: After 7 seconds, display a message
    else if (scene7ElapsedTime < 12000) {

      if (machine_tracks[16]) {      // Note: Track 21 is at index 20 in a zero-based array
        mp3_machine.play(17);        // machine malfunction
        machine_tracks[16] = false;  // Set to false to prevent re-playing
      }
      machine.setPixelColor(25, machine.Color(204, 153, 0));  // yellow color
      machine.show();
      lcd.setCursor(0, 1);
      lcd.print("police scene     ");
      // Additional actions for Step 2 here
      flickerSpeed = 200;
      bombs = false;
      mp3_ambient.play(1);
      time_circuits.clear();
      time_circuits.show();
      time_display.clear();
      oneeightfourtwo();
    }

    // Step 3: Another action from 8280 seconds
    else if (scene7ElapsedTime < 35000) {
      scene7Active = false;  // Deactivate Scene 7
      scene7 = true;
      ambient_tracks[7] = true;
      scene7Waiting = true;
      Serial.println("Scene 7 Complete");

      Serial.println("sc7 waiting");

      startMillis = millis();  // Update the start time for scene 7
    }


    // Scene 7 waiting period after Scene 7 ends
    if (scene7Waiting && (millis() - startMillis >= 5000)) {  // Adjust waiting duration as needed
      scene7Waiting = false;

      //  lcd.setCursor(0, 0);
      // lcd.print("sc7 waiting     ");
      lcd.setCursor(0, 1);
      lcd.print("sc7 walking scene   ");
      lcd.setCursor(0, 0);
      lcd.print("sc7 waiting        ");
      Serial.println("sc7 waiting");
      Display.setPixelColor(10, 0, 0, 255);  // Change color to zero

      Display.show();
    }
  }


  // Scene 8: Activate with SC5 button
  if (!scene8Active && !digitalRead(SC5)) {
    startMillis = millis();                // Update the start time for scene 5
    Display.setPixelColor(10, 0, 0, 0);    // Change color to zero
    Display.setPixelColor(11, 0, 255, 0);  // Change color to green
    Display.show();
    Serial.println("Scene 8 Activated by SC5");
    machine.setPixelColor(18, 255, 0, 0);  // Set the pixel color      // door locked
    machine.setPixelColor(19, 255, 0, 0);  // Set the pixel color
    machine.setPixelColor(20, 255, 0, 0);  // Set the pixel color
    machine.setPixelColor(21, 255, 0, 0);  // Set the pixel color     // door locked
    for (int i = 3; i <= 10; i++) {
      machine.setPixelColor(i, machine.Color(255, 0, 0));  // red color
      machine.show();
    }
    scene8Active = true;  // Set active to prevent multiple prints
    lcd.setCursor(0, 1);
    lcd.print("pub scene        ");
    lcd.setCursor(0, 0);
    lcd.print("MACHINE: running");

    // Reset flags for flash messages
    flash1Printed = false;
    flash2Printed = false;
    flash3Printed = false;
    flash4Printed = false;
    flash5Printed = false;
    flash6Printed = false;
    flash7Printed = false;
    flash8Printed = false;
    flash9Printed = false;
  }
  // Running Scene 8 with multiple steps
  if (scene8Active) {
    unsigned long scene8ElapsedTime = millis() - startMillis;

    if (scene8ElapsedTime >= 50) {
      if (ambient_tracks[17]) {                   // Note: Track 21 is at index 20 in a zero-based array
        mp3_ambient.play(5);                      // travelling sound
        ambient_tracks[17] = false;               // Set to false to prevent re-playing
        machine.setPixelColor(0, 255, 255, 255);  // Set the pixel color as  relay example for flashes of lights
        machine.show();
        delay(1000);
        machine.setPixelColor(0, 0, 0, 0);  // Set the pixel color
        machine.show();
        pixels.clear();
        pixels.show();
      }
      if (scene8ElapsedTime >= 18000 && !flash1Printed) {
        lcd.setCursor(0, 1);
        lcd.print("flash 2        ");
        Serial.println("flash 2");  // Print flash 1 only once
        digitalWrite(relay2, LOW);
        machine.setPixelColor(0, 255, 255, 255);  // Set the pixel color as  relay example for flashes of lights
        machine.show();
        delay(1000);
        machine.setPixelColor(0, 0, 0, 0);  // Set the pixel color
        machine.show();
        digitalWrite(relay2, HIGH);
        flash1Printed = true;  // Set flag to prevent repeat
      }

      if (scene8ElapsedTime >= 21500 && !flash2Printed) {
        lcd.setCursor(0, 1);
        lcd.print("flash 3       ");
        Serial.println("flash 3");                // Print flash 2 only once
        machine.setPixelColor(0, 255, 255, 255);  // Set the pixel color as  relay example for flashes of lights
        machine.show();
        digitalWrite(relay2, LOW);
        delay(1000);
        machine.setPixelColor(0, 0, 0, 0);  // Set the pixel color
        machine.show();
        digitalWrite(relay2, HIGH);

        flash2Printed = true;  // Set flag to prevent repeat
      }

      if (scene8ElapsedTime >= 25000 && !flash3Printed) {
        lcd.setCursor(0, 1);
        lcd.print("flash 4        ");
        Serial.println("flash 4");                // Print flash 3 only once
        machine.setPixelColor(0, 255, 255, 255);  // Set the pixel color as  relay example for flashes of lights
        machine.show();
        digitalWrite(relay2, LOW);
        delay(1000);
        machine.setPixelColor(0, 0, 0, 0);  // Set the pixel color
        machine.show();
        digitalWrite(relay2, HIGH);
        flash3Printed = true;  // Set flag to prevent repeat
      }


      if (scene8ElapsedTime >= 26500 && !flash4Printed) {
        lcd.setCursor(0, 1);
        lcd.print("flash 5        ");
        Serial.println("flash 5");                // Print flash 3 only once
        machine.setPixelColor(0, 255, 255, 255);  // Set the pixel color as  relay example for flashes of lights
        machine.show();
        digitalWrite(relay2, LOW);
        delay(1000);
        machine.setPixelColor(0, 0, 0, 0);  // Set the pixel color
        machine.show();
        digitalWrite(relay2, HIGH);
        flash4Printed = true;  // Set flag to prevent repeat
      }

      if (scene8ElapsedTime >= 27500 && !flash5Printed) {
        lcd.setCursor(0, 1);
        lcd.print("flash 6        ");
        Serial.println("flash 6");                // Print flash 3 only once
        machine.setPixelColor(0, 255, 255, 255);  // Set the pixel color as  relay example for flashes of lights
        machine.show();
        digitalWrite(relay2, LOW);
        delay(1000);
        machine.setPixelColor(0, 0, 0, 0);  // Set the pixel color
        machine.show();
        digitalWrite(relay2, HIGH);
        flash5Printed = true;  // Set flag to prevent repeat
      }

      if (scene8ElapsedTime >= 30000 && !flash6Printed) {
        lcd.setCursor(0, 1);
        lcd.print("flash 7       ");
        Serial.println("flash 7");                // Print flash 3 only once
        machine.setPixelColor(0, 255, 255, 255);  // Set the pixel color as  relay example for flashes of lights
        machine.show();
        digitalWrite(relay2, LOW);
        delay(1000);
        machine.setPixelColor(0, 0, 0, 0);  // Set the pixel color
        machine.show();
        digitalWrite(relay2, HIGH);
        flash6Printed = true;  // Set flag to prevent repeat
      }

      if (scene8ElapsedTime >= 34000 && !flash7Printed) {
        lcd.setCursor(0, 1);
        lcd.print("flash 8        ");
        Serial.println("flash 8");                // Print flash 3 only once
        machine.setPixelColor(0, 255, 255, 255);  // Set the pixel color as  relay example for flashes of lights
        machine.show();
        digitalWrite(relay2, LOW);
        delay(1000);
        machine.setPixelColor(0, 0, 0, 0);  // Set the pixel color
        machine.show();
        digitalWrite(relay2, HIGH);
        flash7Printed = true;  // Set flag to prevent repeat
      }
      if (scene8ElapsedTime >= 39000 && !flash8Printed) {
        lcd.setCursor(0, 1);
        lcd.print("flash 9        ");
        Serial.println("flash 9");                // Print flash 3 only once
        machine.setPixelColor(0, 255, 255, 255);  // Set the pixel color as  relay example for flashes of lights
        machine.show();
        digitalWrite(relay2, LOW);
        delay(4000);
        machine.setPixelColor(0, 0, 0, 0);  // Set the pixel color
        machine.show();
        flash8Printed = true;  // Set flag to prevent repeat
        mp3_ambient.play(1);
      }
      if (scene8ElapsedTime >= 42000 && !flash8Printed) {
        lcd.setCursor(0, 1);
        lcd.print("flash 9        ");
        Serial.println("flash 9");                // Print flash 3 only once
        machine.setPixelColor(0, 255, 255, 255);  // Set the pixel color as  relay example for flashes of lights
        machine.show();
        delay(3000);
        machine.setPixelColor(0, 0, 0, 0);  // Set the pixel color
        machine.show();

        flash8Printed = true;  // Set flag to prevent repeat
        mp3_ambient.play(1);
      }

      // End Scene 8 after a duration
      if (scene8ElapsedTime >= 45000) {
        candles();
        scene8Active = false;    // Deactivate Scene 8
        scene8Waiting = true;    // Start the waiting period
        startMillis = millis();  // Reset startMillis for the waiting period
      }
    }
  }


  // Scene 8 waiting period after Scene 8 ends
  else if (scene8Waiting && (millis() - startMillis >= 5000)) {
    scene8Waiting = false;
    digitalWrite(relay2, HIGH);
    lcd.setCursor(0, 0);
    lcd.print("sc8 waiting     ");
    lcd.setCursor(0, 1);
    lcd.print("victorian london         ");
    Serial.println("Scene 8 Complete");
    Display.setPixelColor(11, 0, 0, 255);  // Change color to zero
    Display.show();
  }




  // Scene 9: Activate with SC6 button
  if (!scene9Active && !digitalRead(SC6)) {
    startMillis = millis();  // Update the start time for scene 5

    if (ambient_tracks[28]) {  // Note: Track 21 is at index 20 in a zero-based array
      mp3_ambient.play(1);
      ambient_tracks[28] = false;
    }                      // Set to false to prevent re-playing
    scene8Active = false;  // Deactivate Scene 8
    Serial.println("Scene 9 Activated by SC6");
    scene9Active = true;  // Set active to prevent multiple prints
    lcd.setCursor(0, 1);
    lcd.print("home scene      ");
    lcd.setCursor(0, 0);
    lcd.print("MACHINE: running");

    // Reset flags for flash messages
    Printed1 = false;
    Printed2 = false;
    Printed3 = false;
    Printed4 = false;
    Printed5 = false;
    Printed6 = false;
    Printed7 = false;
    Printed8 = false;
    Printed9 = false;
    machine_tracks[24] = true;  // Set to false to prevent re-playing
  }

  // Running Scene 9 with multiple steps
  if (scene9Active) {
    unsigned long scene9ElapsedTime = millis() - startMillis;
    candles();
    istravelling = true;
    if (machine_tracks[26]) {      // Note: Track 21 is at index 20 in a zero-based array
      mp3_machine.play(5);         // boot up sound
      machine_tracks[26] = false;  // Set to false to prevent re-playing
    }

    // First event at 50ms: Play ambient track and flash
    if (scene9ElapsedTime >= 5000) {
      mp3_ambient.setVol(30);
      if (ambient_tracks[20]) {      // Note: Track 21 is at index 20 in a zero-based array
        mp3_ambient.play(19);        //  power back
        ambient_tracks[20] = false;  // Set to false to prevent re-playing
      }
      isglitching = false;
      delay(3000);
      if (ambient_tracks[28]) {  // Note: Track 21 is at index 20 in a zero-based array
        mp3_ambient.play(1);     //ambience
        ambient_tracks[28] = false;
      }  // Set to false to prevent re-playing
      lcd.setCursor(0, 0);
      lcd.print("MACHINE: running");
    }

    // Second event at 19000ms
    if (scene9ElapsedTime >= 5000 && !Printed1) {
      if (ambient_tracks[28]) {  // Note: Track 21 is at index 20 in a zero-based array
        mp3_machine.play(7);
        ambient_tracks[28] = false;
      }
      scene9 = true;
      machine.setPixelColor(18, 0, 255, 0);  // Set the pixel color      // door open
      machine.setPixelColor(19, 0, 255, 0);  // Set the pixel color
      machine.setPixelColor(20, 0, 255, 0);  // Set the pixel color
      machine.setPixelColor(21, 0, 255, 0);  // Set the pixel color     // door open
      mp3_ambient.setVol(30);
      circuit_flick = true;
      time_display.clear();

      Serial.println("Scene 9 Activated ");
      machine.setPixelColor(1, 255, 255, 0);    // Set the pixel color
      machine.setPixelColor(2, 255, 255, 255);  // Set the pixel color
      for (int i = 3; i <= 10; i++) {
        machine.setPixelColor(i, machine.Color(0, 0, 0));  // Green color
      }
      machine.show();
      lcd.setCursor(0, 1);
      lcd.print("Scene 9 Active");
      oneeightfourtwo();
    }


    if (scene9ElapsedTime >= 5000 && !Printed2) {
      mp3_ambient.play(1);  //


      Printed2 = true;  // Set flag to prevent repeat
    }


    if (scene9ElapsedTime >= 10000 && !Printed3) {
      // circuit_flick = true;
      // flickerSpeed = 10;
      Printed3 = true;  // Set flag to prevent repeat
    }

    // below does nothing but to afraid to delete

    if (scene9ElapsedTime >= 10500 && !Printed4) {

      Printed4 = true;  // Set flag to prevent repeat
    }


    // End Scene 9 after 45000ms (45 seconds)
    if (scene9ElapsedTime >= 15000) {
      time_display.clear();
      time_display.show();
      scene9Active = false;    // Deactivate Scene 9
      scene9Waiting = true;    // Start waiting period
      startMillis = millis();  // Reset startMillis for the waiting period
    }
  }

  // Scene 9 waiting period after Scene 9 ends
  if (scene9Waiting && (millis() - startMillis >= 5000)) {  // Adjust waiting duration as needed
    scene9Waiting = false;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("sc9 waiting     ");
    lcd.setCursor(0, 1);
    lcd.print("machine waiting        ");
    ambient_tracks[7] = true;
    oneeightfourtwo();
  }
  //  Serial.println("Scene 10 Complete");




  // Scene 10: Activate with SC7 button


  // Scene activation logic
  if (!scene10Active && !digitalRead(SC7)) {
    activateScene10();
  }

  // Handle time-based updates
  if (scene10Active) {
    handleScene10();
  }

  // Handle flickering
  if (circuit_flick) {
    timecircuit_flicker();
  }
}

void activateScene10() {
  startMillis = millis();  // Save start time for scene
  circuit_flick = true;


  // Play tracks
  if (machine_tracks[24]) {
    mp3_machine.play(24);  // Play door sound
    machine_tracks[24] = false;
  }
  if (ambient_tracks[7]) {
    mp3_ambient.play(8);  // Play travelling sound
    ambient_tracks[7] = false;
  }


  // Set LED colors (door locked) and gauges on
  machine.setPixelColor(1, 255, 255, 0);    // Set the pixel color
  machine.setPixelColor(2, 255, 255, 255);  // Set the pixel color
  machine.setPixelColor(18, 0, 0, 255);
  machine.setPixelColor(19, 0, 0, 255);
  machine.setPixelColor(20, 0, 0, 255);
  machine.setPixelColor(21, 0, 0, 255);
  machine.show();

  scene10Active = true;
  Serial.println("Scene 10 Activated by SC7");

  // Update LCD

  lcd.setCursor(0, 1);
  lcd.print("sc10 travelling");
  timecircuit_flicker();
  flickerSpeed = 10;
  istravelling = false;  // Reset travelling state
}

void handleScene10() {
  unsigned long currentMillis = millis();
  unsigned long scene10ElapsedTime = currentMillis - startMillis;
  for (int i = 11; i < 17; i++) {
    machine.setPixelColor(i, 255, 255, 255);
  }
  machine.show();
  // Stop circuit flickering and transition after 5 seconds
  if (scene10ElapsedTime >= 25000 && circuit_flick) {
    time_display.clear();
    twentyfive();                             // Call your next scene or function
    circuit_flick = false;                    // Stop flickering
    machine_tracks[26] = true;                // Set to true to play again
    machine.setPixelColor(1, 255, 255, 0);    // Set the pixel color
    machine.setPixelColor(2, 255, 255, 255);  // Set the pixel color
    machine.show();
    for (int i = 22; i < 28; i++) {
      machine.setPixelColor(i, 255, 255, 255);
    }
    machine.show();
    delay(8000);

    if (machine_tracks[26]) {      // Note: Tracks is in a zero-based array
      mp3_machine.play(26);        // blast shield sound
      machine_tracks[26] = false;  // Set to false to prevent re-playing
    }

    machine.setPixelColor(18, 0, 255, 0);
    machine.setPixelColor(19, 0, 255, 0);
    machine.setPixelColor(20, 0, 255, 0);
    machine.setPixelColor(21, 0, 255, 0);
    machine.show();
    machineStarted = false;
    lcd.setCursor(0, 0);
    lcd.print("machine-started   ");
    lcd.setCursor(0, 1);
    lcd.print("start/scene select   ");
  }
}