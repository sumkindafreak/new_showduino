#ifndef SHOWDUINO_PIXEL_FX_H
#define SHOWDUINO_PIXEL_FX_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/*
 * Shared Showduino pixel-effect vocabulary.
 *
 * This header deliberately contains no hardware driver. The P4 local pixel
 * line and later C3 Pixel Nodes can share the exact same effect IDs and segment
 * state while using their own NeoPixel transport implementation.
 */

enum class ShowduinoPixelFx : uint8_t {
  Off = 0,
  Solid,
  FadeIn,
  FadeOut,
  Pulse,
  Breathe,
  Flicker,
  Candle,
  Fire,
  Lightning,
  Strobe,
  RandomStrobe,
  Chase,
  Bounce,
  Comet,
  Wipe,
  ReverseWipe,
  Build,
  Sparkle,
  Twinkle,
  Glitch,
  Warning,
  Portal,
  Rainbow,
  CustomSequence,
  Count
};

struct ShowduinoPixelColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

struct ShowduinoPixelSegmentState {
  bool configured;
  bool active;
  uint16_t start;
  uint16_t count;
  ShowduinoPixelFx effect;
  ShowduinoPixelColor primary;
  ShowduinoPixelColor secondary;
  uint8_t brightness;     /* 0..255 */
  uint8_t speed;          /* 1..100 */
  uint8_t intensity;      /* 0..100 */
  uint8_t randomness;     /* 0..100 */
  bool reverse;
  uint32_t durationMs;    /* 0 = indefinite */
  uint32_t startedMs;
  uint32_t lastStepMs;
  uint32_t phase;
  uint32_t aux;
};

inline const char *showduinoPixelFxName(ShowduinoPixelFx fx) {
  switch (fx) {
    case ShowduinoPixelFx::Off:            return "OFF";
    case ShowduinoPixelFx::Solid:          return "SOLID";
    case ShowduinoPixelFx::FadeIn:         return "FADE_IN";
    case ShowduinoPixelFx::FadeOut:        return "FADE_OUT";
    case ShowduinoPixelFx::Pulse:          return "PULSE";
    case ShowduinoPixelFx::Breathe:        return "BREATHE";
    case ShowduinoPixelFx::Flicker:        return "FLICKER";
    case ShowduinoPixelFx::Candle:         return "CANDLE";
    case ShowduinoPixelFx::Fire:           return "FIRE";
    case ShowduinoPixelFx::Lightning:      return "LIGHTNING";
    case ShowduinoPixelFx::Strobe:         return "STROBE";
    case ShowduinoPixelFx::RandomStrobe:   return "RANDOM_STROBE";
    case ShowduinoPixelFx::Chase:          return "CHASE";
    case ShowduinoPixelFx::Bounce:         return "BOUNCE";
    case ShowduinoPixelFx::Comet:          return "COMET";
    case ShowduinoPixelFx::Wipe:           return "WIPE";
    case ShowduinoPixelFx::ReverseWipe:    return "REVERSE_WIPE";
    case ShowduinoPixelFx::Build:          return "BUILD";
    case ShowduinoPixelFx::Sparkle:        return "SPARKLE";
    case ShowduinoPixelFx::Twinkle:        return "TWINKLE";
    case ShowduinoPixelFx::Glitch:         return "GLITCH";
    case ShowduinoPixelFx::Warning:        return "WARNING";
    case ShowduinoPixelFx::Portal:         return "PORTAL";
    case ShowduinoPixelFx::Rainbow:        return "RAINBOW";
    case ShowduinoPixelFx::CustomSequence: return "CUSTOM_SEQUENCE";
    default:                                return "UNKNOWN";
  }
}

inline bool showduinoPixelFxFromName(const char *name, ShowduinoPixelFx *out) {
  if (!name || !out) return false;

  struct Entry { const char *name; ShowduinoPixelFx fx; };
  static const Entry entries[] = {
    {"OFF", ShowduinoPixelFx::Off},
    {"BLACKOUT", ShowduinoPixelFx::Off},
    {"SOLID", ShowduinoPixelFx::Solid},
    {"FADE_IN", ShowduinoPixelFx::FadeIn},
    {"FADE_OUT", ShowduinoPixelFx::FadeOut},
    {"PULSE", ShowduinoPixelFx::Pulse},
    {"BREATHE", ShowduinoPixelFx::Breathe},
    {"FLICKER", ShowduinoPixelFx::Flicker},
    {"CANDLE", ShowduinoPixelFx::Candle},
    {"FIRE", ShowduinoPixelFx::Fire},
    {"LIGHTNING", ShowduinoPixelFx::Lightning},
    {"STROBE", ShowduinoPixelFx::Strobe},
    {"RANDOM_STROBE", ShowduinoPixelFx::RandomStrobe},
    {"CHASE", ShowduinoPixelFx::Chase},
    {"BOUNCE", ShowduinoPixelFx::Bounce},
    {"COMET", ShowduinoPixelFx::Comet},
    {"WIPE", ShowduinoPixelFx::Wipe},
    {"REVERSE_WIPE", ShowduinoPixelFx::ReverseWipe},
    {"BUILD", ShowduinoPixelFx::Build},
    {"SPARKLE", ShowduinoPixelFx::Sparkle},
    {"TWINKLE", ShowduinoPixelFx::Twinkle},
    {"GLITCH", ShowduinoPixelFx::Glitch},
    {"WARNING", ShowduinoPixelFx::Warning},
    {"WARNING_RED", ShowduinoPixelFx::Warning},
    {"PORTAL", ShowduinoPixelFx::Portal},
    {"PORTAL_GLOW", ShowduinoPixelFx::Portal},
    {"RAINBOW", ShowduinoPixelFx::Rainbow},
    {"CUSTOM_SEQUENCE", ShowduinoPixelFx::CustomSequence}
  };

  for (size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); ++i) {
    if (strcmp(name, entries[i].name) == 0) {
      *out = entries[i].fx;
      return true;
    }
  }
  return false;
}

inline ShowduinoPixelSegmentState showduinoPixelDefaultSegment() {
  ShowduinoPixelSegmentState s{};
  s.configured = false;
  s.active = false;
  s.start = 0;
  s.count = 0;
  s.effect = ShowduinoPixelFx::Off;
  s.primary = {255, 255, 255};
  s.secondary = {0, 0, 0};
  s.brightness = 255;
  s.speed = 50;
  s.intensity = 80;
  s.randomness = 70;
  s.reverse = false;
  s.durationMs = 0;
  s.startedMs = 0;
  s.lastStepMs = 0;
  s.phase = 0;
  s.aux = 0;
  return s;
}

#endif /* SHOWDUINO_PIXEL_FX_H */
