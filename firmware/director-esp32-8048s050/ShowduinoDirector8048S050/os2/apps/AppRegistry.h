#pragma once

#include "App.h"
#include "DashboardApp.h"
#include "LibraryApp.h"

namespace Os2 {

inline bool registerApp(IApp *app) {
  return apps().add(app);
}

inline void registerDefaultApps() {
  static bool once = false;
  if (once) return;
  once = true;

  static DashboardApp dashboard;
  static LibraryApp library;
  registerApp(&dashboard);  /* primary operator workspace */
  registerApp(&library);    /* production catalogue — not runtime */

  /* Future:
   *   registerApp(&lighting);
   *   registerApp(&audio);
   *   registerApp(&devices);
   *   registerApp(&settings);
   *   registerApp(&safety);
   */
}

}  // namespace Os2