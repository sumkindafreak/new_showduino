#pragma once

#include <stdint.h>

/*
 * Showduino OS visual language.
 *
 * These tokens come from DirectorUnlockScreen — the canonical visual
 * reference for the Director: near-black green-tinted surfaces, technical
 * lime accents, restrained glow, high-contrast text, and safety colours that
 * remain visually separate from normal operation.
 */
namespace ShowduinoPalette {
static constexpr uint32_t Background   = 0x020502;
static constexpr uint32_t Panel        = 0x071007;
static constexpr uint32_t PanelRaised  = 0x0C180B;
static constexpr uint32_t Elevated     = 0x102410;
static constexpr uint32_t Accent       = 0x84FF22;
static constexpr uint32_t AccentBright = 0xB5FF6E;
static constexpr uint32_t AccentDark   = 0x2E690D;
static constexpr uint32_t AccentDim    = 0x17370B;
static constexpr uint32_t Text         = 0xF3F6F1;
static constexpr uint32_t Muted        = 0xA9B4A4;
static constexpr uint32_t Disabled     = 0x4A5056;
static constexpr uint32_t Warn         = 0xFFD54A;
static constexpr uint32_t Pending      = 0x3B82F6;
static constexpr uint32_t Success      = 0x84FF22;
static constexpr uint32_t Danger       = 0xFF4545;
static constexpr uint32_t DangerDark   = 0x450A0A;
static constexpr uint32_t DangerPanel  = 0x3A0B0B;
static constexpr uint32_t DangerText   = 0xFECACA;
}