#pragma once

#include <stdint.h>

/**
 * Showduino OS Compatibility Contract
 *
 * Every public interface is an API — even when only used in-tree today.
 * Breaking changes require a new major contract version (v2), never a
 * silent reinterpretation of existing methods or fields.
 *
 * Multiple clients (Director, simulator, web UI) consume these contracts.
 */

namespace Os2 {
namespace Api {

/* Contract majors — bump only when meaning or required shape changes. */
static constexpr uint16_t CommandService       = 1;
static constexpr uint16_t ShowService          = 1;
static constexpr uint16_t AssetService         = 1;
static constexpr uint16_t NetworkService       = 1;
static constexpr uint16_t DeviceService        = 1;
static constexpr uint16_t SessionService       = 1;
static constexpr uint16_t EventBus             = 1;
static constexpr uint16_t ProductionManifest   = 1;
static constexpr uint16_t ThemeEngine          = 1;
static constexpr uint16_t Shell                = 1;
static constexpr uint16_t AppContract          = 1;

/** Platform aggregate — clients may refuse mismatched majors. */
struct Compatibility {
  uint16_t commandService;
  uint16_t showService;
  uint16_t assetService;
  uint16_t networkService;
  uint16_t deviceService;
  uint16_t sessionService;
  uint16_t eventBus;
  uint16_t productionManifest;
  uint16_t themeEngine;
  uint16_t shell;
  uint16_t appContract;
};

inline Compatibility current() {
  Compatibility c{};
  c.commandService     = CommandService;
  c.showService        = ShowService;
  c.assetService       = AssetService;
  c.networkService     = NetworkService;
  c.deviceService       = DeviceService;
  c.sessionService     = SessionService;
  c.eventBus           = EventBus;
  c.productionManifest = ProductionManifest;
  c.themeEngine        = ThemeEngine;
  c.shell              = Shell;
  c.appContract        = AppContract;
  return c;
}

}  // namespace Api
}  // namespace Os2