#pragma once
#include <Arduino.h>
void mosfetRecoveryLoad(uint8_t levels[4]);
bool mosfetRecoverySave(const uint8_t levels[4]);
