#pragma once

#include <Arduino.h>

/* BankOfDad-style auto backlight: full -> dim -> off; touch wakes.
 * Timeout minutes come from DirectorConfig.screenTimeoutMinutes (0 = never).
 *
 * Boot: start OFF. Call backlightRevealNow() only after the first complete
 * boot frame is in the panel framebuffer. Reveal is immediate (no LCD fade).
 */

#define BL_PWM_MAX       1023
#define BL_PWM_DIM_FLOOR 120
#define BL_WAKE_LATCH_MS 45000UL

void backlightInit(uint8_t pin);

/** Apply saved config. timeoutMinutes=0 disables auto dim/off. brightness 0-255. */
void backlightConfigure(uint8_t timeoutMinutes, uint8_t brightness255);

/** Hold the panel dark until the first complete boot frame is ready. */
bool backlightIsHeldOff();

/** Turn the backlight on immediately at the configured brightness. */
void backlightRevealNow();

void backlightNotifyActivity();
void backlightTick(uint32_t nowMs);

bool backlightIsOn();
bool backlightAutoEnabled();
uint8_t backlightTimeoutMinutes();
uint8_t backlightBrightness();

const char *backlightStatusText();

/** Force on/off without changing auto policy (legacy helper). */
void backlightSet(bool on);
