#include "SX1509Device.h"

namespace {
constexpr uint8_t REG_PULL_UP_B = 0x06;
constexpr uint8_t REG_DIR_B = 0x0E;
constexpr uint8_t REG_DATA_B = 0x10;
}

ShowduinoSX1509::ShowduinoSX1509(uint8_t address, Role role)
    : _address(address), _role(role), _online(false), _configuredInputs(0), _configuredOutputs(0) {
  for (uint8_t pin = 0; pin < 16; ++pin) {
    _inputs[pin] = {nullptr, GENERIC_DIGITAL, true, false, false, 0};
    _outputNames[pin] = nullptr;
    _outputStates[pin] = false;
    _outputActiveLow[pin] = false;
  }
}

bool ShowduinoSX1509::probe() {
  Wire.beginTransmission(_address);
  return Wire.endTransmission() == 0;
}

bool ShowduinoSX1509::writeReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(_address);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool ShowduinoSX1509::readReg(uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(_address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(_address, static_cast<uint8_t>(1)) != 1) return false;
  value = Wire.read();
  return true;
}

bool ShowduinoSX1509::readWord(uint8_t regB, uint16_t &value) {
  uint8_t b = 0;
  uint8_t a = 0;
  if (!readReg(regB, b) || !readReg(regB + 1, a)) return false;
  value = (static_cast<uint16_t>(b) << 8) | a;
  return true;
}

bool ShowduinoSX1509::writeWord(uint8_t regB, uint16_t value) {
  return writeReg(regB, static_cast<uint8_t>(value >> 8)) &&
         writeReg(regB + 1, static_cast<uint8_t>(value & 0xFF));
}

bool ShowduinoSX1509::modifyWord(uint8_t regB, uint8_t pin, bool value) {
  if (pin > 15) return false;
  uint16_t word = 0;
  if (!readWord(regB, word)) return false;
  if (value) word |= static_cast<uint16_t>(1U << pin);
  else word &= static_cast<uint16_t>(~(1U << pin));
  return writeWord(regB, word);
}

bool ShowduinoSX1509::begin() {
  _online = probe();
  Serial.printf("[SX1509] 0x%02X %s role=%s\n", _address,
                _online ? "ONLINE" : "NOT FOUND",
                _role == INPUT_BOARD ? "INPUT" : "OUTPUT");
  return _online;
}

bool ShowduinoSX1509::configureInput(uint8_t pin, const char *name, InputKind kind,
                                     bool activeLow, bool enablePullup) {
  if (!_online || _role != INPUT_BOARD || pin > 15 || name == nullptr) return false;
  if (!modifyWord(REG_DIR_B, pin, true)) return false;
  if (!modifyWord(REG_PULL_UP_B, pin, enablePullup)) return false;

  uint16_t data = 0xFFFF;
  if (!readWord(REG_DATA_B, data)) return false;
  const bool electricalHigh = (data & (1U << pin)) != 0;
  const bool active = activeLow ? !electricalHigh : electricalHigh;

  _inputs[pin] = {name, kind, activeLow, active, active, millis()};
  _configuredInputs |= static_cast<uint16_t>(1U << pin);
  Serial.printf("[SX1509] 0x%02X IN%u %s (%s)\n", _address, pin, name, kindName(kind));
  return true;
}

bool ShowduinoSX1509::configureOutput(uint8_t pin, const char *name, bool initialOn, bool activeLow) {
  if (!_online || _role != OUTPUT_BOARD || pin > 15 || name == nullptr) return false;

  _outputActiveLow[pin] = activeLow;
  const bool electricalHigh = activeLow ? !initialOn : initialOn;
  if (!modifyWord(REG_DATA_B, pin, electricalHigh)) return false;
  if (!modifyWord(REG_DIR_B, pin, false)) return false;

  _outputNames[pin] = name;
  _outputStates[pin] = initialOn;
  _configuredOutputs |= static_cast<uint16_t>(1U << pin);
  Serial.printf("[SX1509] 0x%02X OUT%u %s initial=%s\n", _address, pin, name, initialOn ? "ON" : "OFF");
  return true;
}

void ShowduinoSX1509::emitInputEvent(uint8_t pin, bool active) {
  const char *name = _inputs[pin].name ? _inputs[pin].name : "UNNAMED";
  Serial.printf("INPUT:%s:%s\n", name, active ? "ACTIVE" : "INACTIVE");
  if (_inputs[pin].kind == BUTTON) {
    Serial.printf("INPUT:%s:%s\n", name, active ? "PRESSED" : "RELEASED");
  }
}

void ShowduinoSX1509::update() {
  if (!_online || _role != INPUT_BOARD || _configuredInputs == 0) return;

  uint16_t data = 0;
  if (!readWord(REG_DATA_B, data)) {
    _online = false;
    Serial.printf("[SX1509] 0x%02X OFFLINE: input read failed\n", _address);
    return;
  }

  const uint32_t now = millis();
  for (uint8_t pin = 0; pin < 16; ++pin) {
    if ((_configuredInputs & (1U << pin)) == 0) continue;
    const bool electricalHigh = (data & (1U << pin)) != 0;
    const bool active = _inputs[pin].activeLow ? !electricalHigh : electricalHigh;

    if (active != _inputs[pin].rawActive) {
      _inputs[pin].rawActive = active;
      _inputs[pin].rawChangedMs = now;
    }
    if (active != _inputs[pin].stableActive && now - _inputs[pin].rawChangedMs >= DEBOUNCE_MS) {
      _inputs[pin].stableActive = active;
      emitInputEvent(pin, active);
    }
  }
}

bool ShowduinoSX1509::inputActive(uint8_t pin) const {
  return pin < 16 && (_configuredInputs & (1U << pin)) && _inputs[pin].stableActive;
}

bool ShowduinoSX1509::setOutput(uint8_t pin, bool on) {
  if (!_online || pin > 15 || (_configuredOutputs & (1U << pin)) == 0) return false;
  const bool electricalHigh = _outputActiveLow[pin] ? !on : on;
  if (!modifyWord(REG_DATA_B, pin, electricalHigh)) {
    _online = false;
    return false;
  }
  _outputStates[pin] = on;
  Serial.printf("OUTPUT:%s:%s\n", _outputNames[pin] ? _outputNames[pin] : "UNNAMED", on ? "ON" : "OFF");
  return true;
}

bool ShowduinoSX1509::outputState(uint8_t pin) const {
  return pin < 16 && (_configuredOutputs & (1U << pin)) && _outputStates[pin];
}

void ShowduinoSX1509::allOutputsOff() {
  for (uint8_t pin = 0; pin < 16; ++pin) {
    if (_configuredOutputs & (1U << pin)) setOutput(pin, false);
  }
}

const char *ShowduinoSX1509::kindName(InputKind kind) const {
  switch (kind) {
    case BUTTON: return "BUTTON";
    case PIR: return "PIR";
    case REED: return "REED";
    case LIMIT_SWITCH: return "LIMIT";
    case PRESSURE_MAT: return "PRESSURE_MAT";
    case BEAM_BREAK: return "BEAM_BREAK";
    default: return "DIGITAL";
  }
}

void ShowduinoSX1509::printStatus() const {
  Serial.printf("[SX1509] 0x%02X %s role=%s\n", _address, _online ? "ONLINE" : "OFFLINE",
                _role == INPUT_BOARD ? "INPUT" : "OUTPUT");
  if (_role == INPUT_BOARD) {
    for (uint8_t pin = 0; pin < 16; ++pin) {
      if (_configuredInputs & (1U << pin)) {
        Serial.printf("  IN%u %-18s %-12s %s\n", pin, _inputs[pin].name, kindName(_inputs[pin].kind),
                      _inputs[pin].stableActive ? "ACTIVE" : "INACTIVE");
      }
    }
  } else {
    for (uint8_t pin = 0; pin < 16; ++pin) {
      if (_configuredOutputs & (1U << pin)) {
        Serial.printf("  OUT%u %-17s %s\n", pin, _outputNames[pin], _outputStates[pin] ? "ON" : "OFF");
      }
    }
  }
}
