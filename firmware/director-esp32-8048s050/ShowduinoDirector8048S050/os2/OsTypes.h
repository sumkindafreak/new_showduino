#pragma once

#include <stdint.h>

/**
 * Showduino OS 2.0 — shared types.
 * Foundation Complete — see os2/Foundation.h
 *
 * Principle: nothing appears by accident.
 * Status uses exactly five levels. No other semantic colours in apps.
 */
namespace Os2 {

enum class StatusLevel : uint8_t {
  Inactive = 0,  /* Grey  — not in use */
  Working  = 1,  /* Blue  — in progress */
  Healthy  = 2,  /* Green — OK */
  Warning  = 3,  /* Amber — attention */
  Critical = 4,  /* Red   — fault / emergency */
};

enum class AppId : uint8_t {
  Dashboard = 0,
  Library,       /* Productions / scenes / templates — not just "shows" */
  Lighting,
  Audio,
  Effects,
  Assets,
  Network,
  Devices,
  Diagnostics,
  Safety,
  Settings,
  Count
};

enum class PanelKind : uint8_t {
  Workspace = 0,
  Inspector,
  Overlay,
  System,
};

}  // namespace Os2