#include <Arduino.h>
#include <Wire.h>

// Standalone Showduino Plug-in Bus v1 commissioning firmware.
// ESP32-P4: SDA GPIO7, SCL GPIO8, 100 kHz, 3.3 V logic.
// The Waveshare board already has I2C pull-ups: never add 5 V pull-ups.

#include "../../stage-engine-p4/ShowduinoStageEngineP4/src/plugin/SX1509Device.h"

static constexpr uint8_t I2C_SDA_PIN = 7;
static constexpr uint8_t I2C_SCL_PIN = 8;
static constexpr uint32_t I2C_FREQUENCY = 100000UL;
static constexpr uint8_t SX1509_INPUT_ADDRESS = 0x3E;
static constexpr uint8_t SX1509_OUTPUT_ADDRESS = 0x3F;

ShowduinoSX1509 inputBoard(SX1509_INPUT_ADDRESS, ShowduinoSX1509::INPUT_BOARD);
ShowduinoSX1509 outputBoard(SX1509_OUTPUT_ADDRESS, ShowduinoSX1509::OUTPUT_BOARD);
String serialLine;

void scanBus() {
  uint8_t found = 0;
  Serial.println("[I2C] Scanning...");
  for (uint8_t address = 0x08; address <= 0x77; ++address) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      ++found;
      Serial.printf("[I2C] Found device at 0x%02X", address);
      if (address == 0x18) Serial.print(" (onboard ES8311 expected)");
      if (address == SX1509_INPUT_ADDRESS) Serial.print(" (SX1509 INPUT expected)");
      if (address == SX1509_OUTPUT_ADDRESS) Serial.print(" (SX1509 OUTPUT expected)");
      Serial.println();
    }
  }
  Serial.printf("[I2C] Scan complete: %u device(s)\n", found);
}

void configureInputs() {
  inputBoard.configureInput(0, "BUTTON_1", ShowduinoSX1509::BUTTON);
  inputBoard.configureInput(1, "BUTTON_2", ShowduinoSX1509::BUTTON);
  inputBoard.configureInput(2, "PIR_1", ShowduinoSX1509::PIR, false, false);
  inputBoard.configureInput(3, "PIR_2", ShowduinoSX1509::PIR, false, false);
  inputBoard.configureInput(4, "DOOR_REED", ShowduinoSX1509::REED);
  inputBoard.configureInput(5, "LIMIT_SWITCH", ShowduinoSX1509::LIMIT_SWITCH);
  inputBoard.configureInput(6, "PRESSURE_MAT", ShowduinoSX1509::PRESSURE_MAT);
  inputBoard.configureInput(7, "BEAM_BREAK", ShowduinoSX1509::BEAM_BREAK);
  inputBoard.configureInput(8, "INPUT_8");
  inputBoard.configureInput(9, "INPUT_9");
  inputBoard.configureInput(10, "INPUT_10");
  inputBoard.configureInput(11, "INPUT_11");
  inputBoard.configureInput(12, "INPUT_12");
  inputBoard.configureInput(13, "INPUT_13");
  inputBoard.configureInput(14, "INPUT_14");
  inputBoard.configureInput(15, "INPUT_15");
}

void configureOutputs() {
  outputBoard.configureOutput(0, "LED_1");
  outputBoard.configureOutput(1, "LED_2");
  outputBoard.configureOutput(2, "RELAY_TRIGGER_1");
  outputBoard.configureOutput(3, "RELAY_TRIGGER_2");
  outputBoard.configureOutput(4, "MOSFET_TRIGGER_1");
  outputBoard.configureOutput(5, "MOSFET_TRIGGER_2");
  outputBoard.configureOutput(6, "WARNING_LAMP");
  outputBoard.configureOutput(7, "BUZZER_TRIGGER");
  outputBoard.configureOutput(8, "PROP_ENABLE_1");
  outputBoard.configureOutput(9, "PROP_ENABLE_2");
  outputBoard.configureOutput(10, "OUTPUT_10");
  outputBoard.configureOutput(11, "OUTPUT_11");
  outputBoard.configureOutput(12, "OUTPUT_12");
  outputBoard.configureOutput(13, "OUTPUT_13");
  outputBoard.configureOutput(14, "OUTPUT_14");
  outputBoard.configureOutput(15, "OUTPUT_15");
}

void printHelp() {
  Serial.println("\nCommands: HELP | SCAN | STATUS | ON:n | OFF:n | ALL:OFF | TEST:OUTPUTS");
}

void testOutputs() {
  if (!outputBoard.online()) {
    Serial.println("[TEST] SX1509 output board is offline");
    return;
  }
  Serial.println("[TEST] Walking all outputs");
  for (uint8_t pin = 0; pin < 16; ++pin) {
    outputBoard.setOutput(pin, true);
    delay(150);
    outputBoard.setOutput(pin, false);
  }
  Serial.println("[TEST] Output walk complete");
}

bool parsePin(const String &text, uint8_t &pin) {
  if (text.length() == 0) return false;
  for (size_t i = 0; i < text.length(); ++i) if (!isDigit(text[i])) return false;
  const int value = text.toInt();
  if (value < 0 || value > 15) return false;
  pin = static_cast<uint8_t>(value);
  return true;
}

void handleCommand(String command) {
  command.trim();
  command.toUpperCase();

  if (command == "HELP") printHelp();
  else if (command == "SCAN") scanBus();
  else if (command == "STATUS") {
    inputBoard.printStatus();
    outputBoard.printStatus();
  } else if (command == "ALL:OFF") outputBoard.allOutputsOff();
  else if (command == "TEST:OUTPUTS") testOutputs();
  else if (command.startsWith("ON:")) {
    uint8_t pin;
    if (parsePin(command.substring(3), pin)) outputBoard.setOutput(pin, true);
    else Serial.println("ERR: output must be 0..15");
  } else if (command.startsWith("OFF:")) {
    uint8_t pin;
    if (parsePin(command.substring(4), pin)) outputBoard.setOutput(pin, false);
    else Serial.println("ERR: output must be 0..15");
  } else if (command.length()) {
    Serial.println("ERR:UNKNOWN_COMMAND");
  }
}

void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println("\n========================================");
  Serial.println(" SHOWDUINO PLUG-IN BUS v1 COMMISSIONING");
  Serial.println("========================================");

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQUENCY);
  Wire.setTimeOut(50);
  delay(20);
  scanBus();

  if (inputBoard.begin()) configureInputs();
  if (outputBoard.begin()) configureOutputs();

  printHelp();
  Serial.println("[READY] Monitoring inputs");
}

void loop() {
  inputBoard.update();

  while (Serial.available()) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\n' || c == '\r') {
      if (serialLine.length()) {
        handleCommand(serialLine);
        serialLine = "";
      }
    } else if (serialLine.length() < 80) {
      serialLine += c;
    }
  }

  delay(2);
}
