#ifndef SHOWDUINO_COMMAND_VALIDATOR_H
#define SHOWDUINO_COMMAND_VALIDATOR_H

#include "ShowCommand.h"

struct CommandValidationResult {
  bool ok = false;
  char error[96] = {0};
};

class CommandValidator {
 public:
  CommandValidationResult validate(const ShowCommand &cmd) const;
};

#endif