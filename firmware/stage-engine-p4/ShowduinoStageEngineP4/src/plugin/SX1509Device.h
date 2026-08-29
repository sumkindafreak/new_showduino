#pragma once

#include <Arduino.h>
#include <Wire.h>

class ShowduinoSX1509 {
 public:
  enum Role : uint8_t { INPUT_BOARD, OUTPUT_BOARD };
  enum InputKind : uint8_t { BUTTON, PIR, REED, LIMIT_SWITCH, PRESSURE_MAT, BEAM_BREAK, GENERIC_DIGITAL };

  struct InputChannel {
    const char *name;
    InputKind kind;
    bool activeLow;
    bool rawActive;
    bool stableActive;
    uint32_t rawChangedMs;
  };

  ShowduinoSX1509(uint8_t address, Role role);

  bool begin();
  void update();
  bool online() const { return _online; }
  uint8_t address() const { return _address; }

  bool configureInput(uint8_t pin, const char *name, InputKind kind = GENERIC_DIGITAL,
                      bool activeLow = true, bool enablePullup = true);
  bool inputActive(uint8_t pin) const;

  bool configureOutput(uint8_t pin, const char *name, bool initialOn = false,
                       bool activeLow = false);
  bool setOutput(uint8_t pin, bool on);
  bool outputState(uint8_t pin) const;
  void allOutputsOff();
  void printStatus() const;

 private:
  static constexpr uint32_t DEBOUNCE_MS = 30UL;

  uint8_t _address;
  Role _role;
  bool _online;
  InputChannel _inputs[16];
  const char *_outputNames[16];
  bool _outputStates[16];
  bool _outputActiveLow[16];
  uint16_t _configuredInputs;
  uint16_t _configuredOutputs;

  bool probe();
  bool readReg(uint8_t reg, uint8_t &value);
  bool writeReg(uint8_t reg, uint8_t value);
  bool readWord(uint8_t regB, uint16_t &value);
  bool writeWord(uint8_t regB, uint16_t value);
  bool modifyWord(uint8_t regB, uint8_t pin, bool value);
  void emitInputEvent(uint8_t pin, bool active);
  const char *kindName(InputKind kind) const;
};
