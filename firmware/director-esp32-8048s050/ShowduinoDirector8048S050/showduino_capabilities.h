#ifndef SHOWDUINO_CAPABILITIES_H
#define SHOWDUINO_CAPABILITIES_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Hardware capability flags for the Director UI.
 *
 * Pages and controls that need missing hardware are disabled or hidden.
 * Future pages stay in the architecture even when a flag is false.
 *
 * These flags describe the fabric / show engine side, not decorative desk LEDs.
 * Update them when real discovery / HELLO data arrives.
 */
struct ShowduinoCapabilities {
  bool relay;     /* Relay output node(s) present */
  bool mosfet;    /* MOSFET / high-current driver node(s) present */
  bool neopixel;  /* NeoPixel / LED strip node(s) present */
  bool audio;     /* Audio playback node / P4 local audio present */
  bool dmx;       /* DMX output present */
};

/**
 * Default bring-up flags.
 * Relay is expected on the current fabric; other outputs stay off until discovered.
 */
inline ShowduinoCapabilities showduino_capabilities_defaults(void) {
  ShowduinoCapabilities caps;
  caps.relay = true;
  caps.mosfet = false;
  caps.neopixel = false;
  caps.audio = false;
  caps.dmx = false;
  return caps;
}

/** True if any physical output channel type is available. */
inline bool showduino_capabilities_any_output(const ShowduinoCapabilities *caps) {
  if (caps == nullptr) {
    return false;
  }
  return caps->relay || caps->mosfet || caps->neopixel || caps->audio || caps->dmx;
}

#endif