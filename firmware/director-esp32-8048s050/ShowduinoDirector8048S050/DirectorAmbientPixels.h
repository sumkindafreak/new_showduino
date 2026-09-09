#ifndef SHOWDUINO_DIRECTOR_AMBIENT_PIXELS_H
#define SHOWDUINO_DIRECTOR_AMBIENT_PIXELS_H

#include <Arduino.h>
#include "BoardConfig.h"
#include "../../../protocol/showduino_show_runtime.h"

/**
 * Director ambient NeoPixel engine.
 *
 * Pages and system events request a named state or a short pulse.
 * This service owns GPIO17 colour, brightness, fades and emergency override.
 * Non-blocking: millis() frames at SHOWDUINO_DIRECTOR_AMBIENT_FRAME_MS.
 *
 * Persistent states remain until a new authoritative state arrives.
 * Temporary events (node discovered, deploy success) restore the previous
 * persistent state. SUCCESS never falls back to READY blindly.
 *
 * Emergency has absolute priority over cosmetic lighting.
 */

enum DirectorAmbientMode : uint8_t {
  DIRECTOR_AMBIENT_OFF = 0,
  DIRECTOR_AMBIENT_BOOT,
  DIRECTOR_AMBIENT_IDLE,
  DIRECTOR_AMBIENT_READY,
  DIRECTOR_AMBIENT_RUNNING,
  DIRECTOR_AMBIENT_WARNING,
  DIRECTOR_AMBIENT_EMERGENCY,
  DIRECTOR_AMBIENT_FAULT,
  DIRECTOR_AMBIENT_SUCCESS,
  DIRECTOR_AMBIENT_DISCOVERY,
  DIRECTOR_AMBIENT_DEGRADED,
  DIRECTOR_AMBIENT_OFFLINE,
  DIRECTOR_AMBIENT_PAUSED,
  DIRECTOR_AMBIENT_STOPPED,
  DIRECTOR_AMBIENT_DEPLOYING
};

enum DirectorAmbientEvent : uint8_t {
  DIRECTOR_AMBIENT_EVT_NONE = 0,
  DIRECTOR_AMBIENT_EVT_NODE_DISCOVERED,
  DIRECTOR_AMBIENT_EVT_DEPLOY_REQUESTED,
  DIRECTOR_AMBIENT_EVT_DEPLOY_SUCCESS,
  DIRECTOR_AMBIENT_EVT_START_REQUESTED,
  DIRECTOR_AMBIENT_EVT_PAUSE_REQUESTED,
  DIRECTOR_AMBIENT_EVT_RESUME_REQUESTED,
  DIRECTOR_AMBIENT_EVT_STOP_REQUESTED,
  DIRECTOR_AMBIENT_EVT_CONNECTION_RESTORED
};

void directorAmbientBegin();
void directorAmbientLoop(uint32_t nowMs);

/** Authoritative Director/P4 context. Does not imply success from a tap. */
void directorAmbientSync(uint8_t linkState,
                         ShowState showState,
                         bool emergencyLocked,
                         bool stageConnected,
                         bool synchronising = false,
                         bool degraded = false);

void directorAmbientSetState(DirectorAmbientMode mode);
void directorAmbientPulse(DirectorAmbientEvent event);
void directorAmbientHoldPresentation(bool hold);
bool directorAmbientPresentationHeld();

void directorAmbientSetEnabled(bool enabled);
bool directorAmbientEnabled();
void directorAmbientSetBrightness(uint8_t brightness);
uint8_t directorAmbientBrightness();
DirectorAmbientMode directorAmbientMode();
bool directorAmbientReady();

void directorAmbientStartLocator(uint32_t nowMs);
bool directorAmbientLocatorActive();

#endif
