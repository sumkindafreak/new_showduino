#ifndef SHOWDUINO_ESTOP_STATUS_PIXELS_H
#define SHOWDUINO_ESTOP_STATUS_PIXELS_H

#include <Arduino.h>
#include "../../../protocol/showduino_emergency_node.h"

/*
 * Dedicated, fixed two-pixel local status / atmosphere line.
 * Does NOT share GPIO, controller, buffer or theatre commands with GPIO2.
 * Call Service only AFTER emergencyProtocolService() has serviced the
 * button and had first opportunity to transmit an emergency assertion.
 */
void emergencyStatusPixelsSafeGpio();
void emergencyStatusPixelsBegin();
void emergencyStatusPixelsService(bool latched, bool systemEmergency,
                                  bool buttonPressed, bool radioFresh,
                                  ShowduinoEmergencyNodeState state,
                                  bool locateActive);
bool emergencyStatusPixelsReady();
const char *emergencyStatusPixelsMode();

#endif
