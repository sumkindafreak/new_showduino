#pragma once

#include <lvgl.h>
#include <string.h>
#include "../OsTypes.h"

namespace Os2 {

class Shell;  /* forward — apps never own shell state */

/**
 * Layer 2 — Application contract.
 *
 * Each app owns: widgets, navigation, data refresh, commands, layout.
 * Nothing leaks into the shell. Shell only hosts the workspace root.
 */
class IApp {
 public:
  virtual ~IApp() = default;

  virtual AppId appId() const = 0;
  virtual const char *id() const = 0;       /* stable string id: "lighting" */
  virtual const char *title() const = 0;    /* human title */
  virtual const char *dockGlyph() const = 0; /* temporary text glyph until icon theme */
  virtual bool showInDock() const { return true; }

  /** Build into the workspace container. Parent is owned by Shell. */
  virtual void onCreate(lv_obj_t *workspace) = 0;
  virtual void onShow() {}
  virtual void onHide() {}
  virtual void onDestroy() {}
  virtual void onTick(uint32_t /*nowMs*/) {}

  /**
   * Optional context-strip actions for the active app.
   * Shell clears the strip before calling this.
   */
  virtual void onBuildContext(lv_obj_t * /*contextStrip*/) {}

  /** Search contributions — return true if query matched and handled. */
  virtual bool onSearch(const char * /*query*/) { return false; }
};

class AppRegistry {
 public:
  static constexpr int kMax = 16;

  static AppRegistry &instance() {
    static AppRegistry r;
    return r;
  }

  bool add(IApp *app) {
    if (!app || count_ >= kMax) return false;
    for (int i = 0; i < count_; ++i) {
      if (apps_[i] == app) return true;
    }
    apps_[count_++] = app;
    return true;
  }

  IApp *find(AppId id) const {
    for (int i = 0; i < count_; ++i) {
      if (apps_[i] && apps_[i]->appId() == id) return apps_[i];
    }
    return nullptr;
  }

  IApp *findByName(const char *id) const {
    if (!id) return nullptr;
    for (int i = 0; i < count_; ++i) {
      if (apps_[i] && apps_[i]->id() && strcmp(apps_[i]->id(), id) == 0) {
        return apps_[i];
      }
    }
    return nullptr;
  }

  IApp *at(int i) const {
    return (i >= 0 && i < count_) ? apps_[i] : nullptr;
  }

  int count() const { return count_; }

 private:
  AppRegistry() = default;
  IApp *apps_[kMax]{};
  int count_ = 0;
};

inline AppRegistry &apps() { return AppRegistry::instance(); }

}  // namespace Os2