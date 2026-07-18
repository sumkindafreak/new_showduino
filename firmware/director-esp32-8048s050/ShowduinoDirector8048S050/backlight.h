#pragma once

#include <Arduino.h>

/* BankOfDad-style auto backlight: full → dim → off; touch wakes.
 * Timeout minutes come from DirectorConfig.screenTimeoutMinutes (0 = never). */

#define BL_PWM_MAX       1023
#define BL_PWM_DIM_FLOOR 120
#define BL_WAKE_LATCH_MS 45000UL

void backlightInit(uint8_t pin);

/** Apply saved config. timeoutMinutes=0 disables auto dim/off. brightness 0–255. */
void backlightConfigure(uint8_t timeoutMinutes, uint8_t brightness255);

void backlightNotifyActivity();
void backlightTick(uint32_t nowMs);

bool backlightIsOn();
bool backlightAutoEnabled();
uint8_t backlightTimeoutMinutes();
uint8_t backlightBrightness();

const char *backlightStatusText();

/** Force on/off without changing auto policy (legacy helper). */
void backlightSet(bool on);
