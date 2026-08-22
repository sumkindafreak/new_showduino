#pragma once

/**
 * Showduino OS â€” Operator Workspace
 * Milestone: Architecture Frozen Â· Phase: Application Development
 * Constitution: os2/Foundation.h Â· ADRs: docs/adr/
 */

#include "Foundation.h"
#include "Compatibility.h"
#include "theme/Theme.h"
#include "events/EventBus.h"
#include "models/Production.h"
#include "services/Service.h"
#include "services/StubServices.h"
#include "services/ShowService.h"
#include "services/NetworkService.h"
#include "services/DeviceService.h"
#include "services/AssetService.h"
#include "services/SessionService.h"
#include "services/CommandService.h"
#include "apps/App.h"
#include "apps/AppRegistry.h"
#include "widgets/Card.h"
#include "wm/PanelManager.h"
#include "shell/Shell.h"
#include "OsBridge.h"

namespace Os2 {

inline void boot() {
  Shell::instance().begin();
}

inline void tick(uint32_t nowMs) {
  Shell::instance().tick(nowMs);
}

inline Shell &shell() { return Shell::instance(); }

}  // namespace Os2