#pragma once
// Proposed design tokens; no global LVGL style mutation or new font assets.
#include <cstdint>
namespace ShowduinoV11 {
struct DesignTokens {
  static constexpr uint32_t background = 0x10151E, panel = 0x1B2634;
  static constexpr uint32_t border = 0x334155, text = 0xF1F5F9;
  static constexpr uint32_t muted = 0x94A3B8, accent = 0x38BDF8;
  static constexpr uint32_t ready = 0x35BD81, warning = 0xE6AA43;
  static constexpr uint32_t danger = 0xF04A48, offline = 0xA7A7A7;
  static constexpr int32_t screenWidth = 800, screenHeight = 480;
  static constexpr int32_t padding = 12, gap = 8, radius = 8, touchTarget = 48;
};
// Layout constants are a proposal. Existing emergency controls retain precedence.
static_assert(DesignTokens::touchTarget >= 44, "Touch targets must remain usable");
} // namespace ShowduinoV11
