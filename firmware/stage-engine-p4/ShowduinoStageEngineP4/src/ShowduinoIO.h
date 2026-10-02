#ifndef SHOWDUINO_GENERIC_IO_H
#define SHOWDUINO_GENERIC_IO_H

#include <Arduino.h>

typedef void (*ShowduinoIOSendFn)(const char *line);

enum class ShowduinoIOMode : uint8_t {
  Disabled = 0,
  Input,
  Output
};

enum class ShowduinoIOPull : uint8_t {
  None = 0,
  Up,
  Down
};

/*
 * Two generic, function-agnostic P4 I/O lines.
 *
 * Physical assignment lives in BoardConfig.h:
 *   line 1 -> GPIO46
 *   line 2 -> GPIO47
 *
 * Safety rules:
 *   - both pins start high-impedance / DISABLED;
 *   - OUTPUT always boots INACTIVE;
 *   - Emergency forces every output INACTIVE;
 *   - clearing Emergency never restores an interrupted output;
 *   - STOP:ALL / SHOW:STOP also force outputs INACTIVE.
 */
void showduinoIOBegin(ShowduinoIOSendFn sendFn);
void showduinoIOReloadConfig();
void showduinoIOService();
void showduinoIOOnEmergency(bool active);
void showduinoIOAllOff(const char *reason);
void showduinoIOPublishState();

/*
 * Command vocabulary:
 *   IO:STATUS
 *   IO:SAVE
 *   IO:ALL:OFF
 *   IO:<1|2>:STATUS
 *   IO:<1|2>:MODE:DISABLED|INPUT|OUTPUT
 *   IO:<1|2>:ACTIVE:HIGH|LOW
 *   IO:<1|2>:PULL:NONE|UP|DOWN
 *   IO:<1|2>:DEBOUNCE:<0-5000>
 *   IO:<1|2>:ON|OFF|TOGGLE
 *   IO:<1|2>:PULSE:<1-3600000>
 */
bool showduinoIOHandleCommand(const char *command, char *reply, size_t replyLen);

#endif /* SHOWDUINO_GENERIC_IO_H */
