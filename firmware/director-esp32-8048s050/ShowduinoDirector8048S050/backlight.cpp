#include "backlight.h"
#include "BoardConfig.h"

static uint8_t s_blPin = 2;

void backlightInit(uint8_t pin) {
  s_blPin = pin;
  pinMode(pin, OUTPUT);
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcAttach(pin, 1000, 10);
  ledcWrite(pin, 1023);
#else
  ledcSetup(0, 1000, 10);
  ledcAttachPin(pin, 0);
  ledcWrite(0, 1023);
#endif
  Serial.println("Backlight: ON");
}

void backlightSet(bool on) {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcWrite(s_blPin, on ? 1023 : 0);
#else
  ledcWrite(0, on ? 1023 : 0);
#endif
  Serial.println(on ? "Backlight: ON" : "Backlight: OFF");
}
