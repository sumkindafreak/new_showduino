#pragma once

#include <lvgl.h>
#include "../theme/Theme.h"
#include "../apps/App.h"
#include "../apps/AppRegistry.h"
#include "../services/StubServices.h"
#include "../events/EventBus.h"
#include "../Foundation.h"
#include "../wm/PanelManager.h"
#include "NotificationManager.h"
#include "OverlayManager.h"

namespace Os2 {

/**
 * Layer 1 — The Shell.
 *
 * This never changes. Ever.
 * It owns absolutely nothing except presentation.
 *
 *   ShowduinoShell
 *    ├── TopBar
 *    ├── Dock
 *    ├── Workspace
 *    ├── ContextPanel
 *    ├── StatusStrip
 *    ├── OverlayManager
 *    └── NotificationManager
 *
 * The shell does not know what Lighting is.
 * It simply hosts apps.
 */
class Shell {
 public:
  static Shell &instance() {
    static Shell s;
    return s;
  }

  void begin() {
    if (ready_) return;

    Theme::engine().begin();
    registerDefaultServices();
    services().beginAll();

    registerDefaultApps();

    buildChrome();
    panels_.bind(workspace_, inspector_, overlayHost_);
    notifications_.bind(notifyHost_);
    overlays_.bind(overlayHost_);

    if (apps().count() > 0) openApp(apps().at(0));

    events().subscribe(Event::LinkChanged, &Shell::onBus, this);
    events().subscribe(Event::EmergencyChanged, &Shell::onBus, this);
    events().subscribe(Event::ShowStarted, &Shell::onBus, this);
    events().subscribe(Event::ShowStopped, &Shell::onBus, this);
    events().subscribe(Event::SystemStatus, &Shell::onBus, this);
    events().subscribe(Event::NetworkHealth, &Shell::onBus, this);
    refreshTopBarStatus();

    sessionService().restore();

    ready_ = true;
  }

  void tick(uint32_t nowMs) {
    if (!ready_) return;
    services().tickAll(nowMs);
    notifications_.tick(nowMs);
    if (active_) active_->onTick(nowMs);
    refreshTopBarClock();
  }

  void openApp(AppId id) { openApp(apps().find(id)); }

  void openApp(IApp *app) {
    if (!app) return;
    if (active_ == app) return;

    if (active_) {
      active_->onHide();
      active_->onDestroy();
    }

    active_ = app;
    lv_obj_t *host = panels_.resetWorkspace();
    app->onCreate(host);
    app->onShow();
    rebuildContext();
    highlightDock(app);
    setTopBarAppTitle(app->title());
    sessionService().setLastApp(app->id());
  }

  IApp *activeApp() const { return active_; }
  PanelManager &panels() { return panels_; }
  NotificationManager &notifications() { return notifications_; }
  OverlayManager &overlays() { return overlays_; }

  lv_obj_t *screen() const { return screen_; }

 private:
  Shell() = default;
  bool ready_ = false;
  IApp *active_ = nullptr;

  lv_obj_t *screen_ = nullptr;
  lv_obj_t *topBar_ = nullptr;
  lv_obj_t *brandLabel_ = nullptr;
  lv_obj_t *statusLabel_ = nullptr;
  lv_obj_t *clockLabel_ = nullptr;
  lv_obj_t *dock_ = nullptr;
  lv_obj_t *workspace_ = nullptr;
  lv_obj_t *inspector_ = nullptr;
  lv_obj_t *bottomStrip_ = nullptr;
  lv_obj_t *contextStrip_ = nullptr;
  lv_obj_t *statusStrip_ = nullptr;
  lv_obj_t *overlayHost_ = nullptr;
  lv_obj_t *notifyHost_ = nullptr;

  lv_obj_t *dockBtns_[AppRegistry::kMax]{};
  int dockCount_ = 0;

  PanelManager panels_;
  NotificationManager notifications_;
  OverlayManager overlays_;

  void buildChrome() {
    Theme::Engine &th = Theme::engine();
    const Theme::Colors &c = th.colors();
    const Theme::Spacing &sp = th.space();

    screen_ = lv_obj_create(nullptr);
    lv_obj_remove_style_all(screen_);
    lv_obj_add_style(screen_, &th.styleScreen, 0);
    lv_obj_set_size(screen_, sp.screenW, sp.screenH);
    lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);

    /* ---- TopBar ---- */
    topBar_ = lv_obj_create(screen_);
    lv_obj_remove_style_all(topBar_);
    lv_obj_set_pos(topBar_, 0, 0);
    lv_obj_set_size(topBar_, sp.screenW, sp.topBarH);
    lv_obj_set_style_bg_color(topBar_, lv_color_hex(c.surface), 0);
    lv_obj_set_style_bg_opa(topBar_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(topBar_, lv_color_hex(c.outline), 0);
    lv_obj_set_style_border_width(topBar_, 1);
    lv_obj_set_style_border_side(topBar_, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_clear_flag(topBar_, LV_OBJ_FLAG_SCROLLABLE);

    brandLabel_ = lv_label_create(topBar_);
    lv_label_set_text(brandLabel_, "SHOWDUINO");
    lv_obj_add_style(brandLabel_, &th.styleTitle, 0);
    lv_obj_set_style_text_color(brandLabel_, lv_color_hex(c.accent), 0);
    lv_obj_align(brandLabel_, LV_ALIGN_LEFT_MID, sp.margin, 0);

    statusLabel_ = lv_label_create(topBar_);
    lv_label_set_text(statusLabel_, "READY");
    lv_obj_add_style(statusLabel_, &th.styleCaption, 0);
    lv_obj_set_style_text_color(statusLabel_, lv_color_hex(c.statusHealthy), 0);
    lv_obj_align(statusLabel_, LV_ALIGN_RIGHT_MID, -96, 0);

    clockLabel_ = lv_label_create(topBar_);
    lv_label_set_text(clockLabel_, "--:--");
    lv_obj_add_style(clockLabel_, &th.styleBody, 0);
    lv_obj_align(clockLabel_, LV_ALIGN_RIGHT_MID, -sp.margin, 0);

    /* ---- Dock (icons only) ---- */
    dock_ = lv_obj_create(screen_);
    lv_obj_remove_style_all(dock_);
    lv_obj_set_pos(dock_, 0, sp.topBarH);
    lv_obj_set_size(dock_, sp.dockW, sp.workspaceH());
    lv_obj_set_style_bg_color(dock_, lv_color_hex(c.surface), 0);
    lv_obj_set_style_bg_opa(dock_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(dock_, lv_color_hex(c.outline), 0);
    lv_obj_set_style_border_width(dock_, 1);
    lv_obj_set_style_border_side(dock_, LV_BORDER_SIDE_RIGHT, 0);
    lv_obj_clear_flag(dock_, LV_OBJ_FLAG_SCROLLABLE);
    buildDockButtons();

    /* ---- Workspace ---- */
    workspace_ = lv_obj_create(screen_);
    lv_obj_remove_style_all(workspace_);
    lv_obj_set_pos(workspace_, sp.workspaceX(), sp.workspaceY());
    lv_obj_set_size(workspace_, sp.workspaceW(), sp.workspaceH());
    lv_obj_set_style_bg_color(workspace_, lv_color_hex(c.background), 0);
    lv_obj_set_style_bg_opa(workspace_, LV_OPA_COVER, 0);
    lv_obj_clear_flag(workspace_, LV_OBJ_FLAG_SCROLLABLE);

    /* ---- Inspector (hidden) ---- */
    inspector_ = lv_obj_create(screen_);
    lv_obj_remove_style_all(inspector_);
    lv_obj_add_style(inspector_, &th.styleSurfaceRaised, 0);
    lv_obj_set_size(inspector_, sp.workspaceW() / 2, sp.workspaceH());
    lv_obj_set_pos(inspector_, sp.screenW, sp.topBarH);
    lv_obj_add_flag(inspector_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(inspector_, LV_OBJ_FLAG_SCROLLABLE);

    /* ---- Bottom: Context + Status ---- */
    bottomStrip_ = lv_obj_create(screen_);
    lv_obj_remove_style_all(bottomStrip_);
    lv_obj_set_pos(bottomStrip_, 0, sp.bottomY());
    lv_obj_set_size(bottomStrip_, sp.screenW, sp.bottomStripH);
    lv_obj_set_style_bg_color(bottomStrip_, lv_color_hex(c.surface), 0);
    lv_obj_set_style_bg_opa(bottomStrip_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(bottomStrip_, lv_color_hex(c.outline), 0);
    lv_obj_set_style_border_width(bottomStrip_, 1);
    lv_obj_set_style_border_side(bottomStrip_, LV_BORDER_SIDE_TOP, 0);
    lv_obj_clear_flag(bottomStrip_, LV_OBJ_FLAG_SCROLLABLE);

    contextStrip_ = lv_obj_create(bottomStrip_);
    lv_obj_remove_style_all(contextStrip_);
    lv_obj_set_pos(contextStrip_, sp.dockW, 0);
    lv_obj_set_size(contextStrip_, sp.workspaceW() - 160, sp.bottomStripH);
    lv_obj_clear_flag(contextStrip_, LV_OBJ_FLAG_SCROLLABLE);

    statusStrip_ = lv_obj_create(bottomStrip_);
    lv_obj_remove_style_all(statusStrip_);
    lv_obj_set_size(statusStrip_, 150, sp.bottomStripH);
    lv_obj_align(statusStrip_, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_clear_flag(statusStrip_, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *sys = lv_label_create(statusStrip_);
    lv_label_set_text(sys, "OS " SHOWDUINO_OS2_VERSION);
    lv_obj_add_style(sys, &th.styleCaption, 0);
    lv_obj_align(sys, LV_ALIGN_CENTER, 0, 0);

    /* ---- Overlay + notify hosts (top layer) ---- */
    overlayHost_ = lv_obj_create(screen_);
    lv_obj_remove_style_all(overlayHost_);
    lv_obj_set_size(overlayHost_, sp.screenW, sp.screenH);
    lv_obj_set_pos(overlayHost_, 0, 0);
    lv_obj_add_flag(overlayHost_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(overlayHost_, LV_OBJ_FLAG_SCROLLABLE);

    notifyHost_ = lv_obj_create(screen_);
    lv_obj_remove_style_all(notifyHost_);
    lv_obj_set_size(notifyHost_, sp.screenW, sp.screenH);
    lv_obj_set_pos(notifyHost_, 0, 0);
    lv_obj_add_flag(notifyHost_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(notifyHost_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(notifyHost_, LV_OBJ_FLAG_SCROLLABLE);

    lv_screen_load(screen_);
  }

  void buildDockButtons() {
    Theme::Engine &th = Theme::engine();
    const Theme::Colors &c = th.colors();
    const Theme::Spacing &sp = th.space();
    const int slot = sp.dockIconSize + sp.gapTight;

    /* Dock knows only the registry — never a hardcoded app list. */
    dockCount_ = 0;
    int visual = 0;
    for (int i = 0; i < apps().count(); ++i) {
      IApp *app = apps().at(i);
      if (!app || !app->showInDock()) continue;
      if (dockCount_ >= AppRegistry::kMax) break;

      lv_obj_t *btn = lv_button_create(dock_);
      lv_obj_remove_style_all(btn);
      lv_obj_add_style(btn, &th.styleButton, 0);
      lv_obj_add_style(btn, &th.styleButtonPressed, LV_STATE_PRESSED);
      lv_obj_set_size(btn, sp.dockIconSize, sp.dockIconSize);
      lv_obj_set_pos(btn, (sp.dockW - sp.dockIconSize) / 2, sp.gap + visual * slot);

      lv_obj_t *lab = lv_label_create(btn);
      lv_label_set_text(lab, app->dockGlyph());
      lv_obj_set_style_text_color(lab, lv_color_hex(c.text), 0);
      lv_obj_center(lab);

      lv_obj_set_user_data(btn, app);
      lv_obj_add_event_cb(btn, dockClicked, LV_EVENT_CLICKED, this);

      dockBtns_[dockCount_++] = btn;
      ++visual;
    }
  }

  static void dockClicked(lv_event_t *e) {
    Shell *self = static_cast<Shell *>(lv_event_get_user_data(e));
    lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
    IApp *app = static_cast<IApp *>(lv_obj_get_user_data(btn));
    if (self) self->openApp(app);
  }

  void highlightDock(IApp *app) {
    Theme::Engine &th = Theme::engine();
    const Theme::Colors &c = th.colors();
    for (int i = 0; i < dockCount_; ++i) {
      lv_obj_t *btn = dockBtns_[i];
      if (!btn) continue;
      bool on = (static_cast<IApp *>(lv_obj_get_user_data(btn)) == app);
      lv_obj_set_style_border_color(btn, lv_color_hex(on ? c.accent : c.outline), 0);
      lv_obj_set_style_border_width(btn, on ? 2 : 1, 0);
    }
  }

  void rebuildContext() {
    if (!contextStrip_) return;
    lv_obj_clean(contextStrip_);
    if (active_) active_->onBuildContext(contextStrip_);
  }

  static void onBus(const EventPayload &ev, void *user) {
    (void)ev;
    Shell *self = static_cast<Shell *>(user);
    if (self) self->refreshTopBarStatus();
  }

  void setTopBarAppTitle(const char * /*title*/) {
    /* Brand stays SHOWDUINO; status/clock own the right side.
       App identity lives in the workspace — nothing appears by accident. */
  }

  void refreshTopBarClock() {
    if (!clockLabel_) return;
    auto *timeSvc = services().get<TimeService>("time");
    if (!timeSvc) return;
    char buf[8];
    timeSvc->formatClock(buf, sizeof(buf));
    lv_label_set_text(clockLabel_, buf);
  }

  void refreshTopBarStatus() {
    if (!statusLabel_) return;
    ShowService *show = services().get<ShowService>("show");
    NetworkService *net = services().get<NetworkService>("network");
    Theme::Engine &th = Theme::engine();
    const Theme::Colors &c = th.colors();

    const char *label = "READY";
    StatusLevel st = StatusLevel::Healthy;
    if (show && show->emergency()) {
      label = "EMERGENCY";
      st = StatusLevel::Critical;
    } else if (net && !net->online()) {
      label = net->linkLabel();
      st = net->status();
    } else if (show && show->running()) {
      label = "RUNNING";
      st = StatusLevel::Working;
    } else if (net) {
      label = "READY";
      st = net->status();
    }
    lv_label_set_text(statusLabel_, label);
    lv_obj_set_style_text_color(statusLabel_, lv_color_hex(c.status(st)), 0);
  }
};

}  // namespace Os2