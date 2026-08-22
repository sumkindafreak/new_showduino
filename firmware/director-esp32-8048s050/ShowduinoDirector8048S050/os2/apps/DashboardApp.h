#pragma once

#include <stdio.h>
#include "App.h"
#include "../theme/Theme.h"
#include "../services/StubServices.h"
#include "../events/EventBus.h"
#include "../widgets/Card.h"

namespace Os2 {

/**
 * Dashboard.app — Mission Control.
 * Owns no data. Subscribes to the Event Bus. Asks services when events fire.
 */
class DashboardApp : public IApp {
 public:
  AppId appId() const override { return AppId::Dashboard; }
  const char *id() const override { return "dashboard"; }
  const char *title() const override { return "Dashboard"; }
  const char *dockGlyph() const override { return "D"; }
  bool showInDock() const override { return true; }

  void onCreate(lv_obj_t *workspace) override {
    root_ = workspace;
    Theme::Engine &th = Theme::engine();
    const Theme::Spacing &sp = th.space();

    lv_obj_t *heading = lv_label_create(workspace);
    lv_label_set_text(heading, "Dashboard");
    lv_obj_add_style(heading, &th.styleTitle, 0);
    lv_obj_set_pos(heading, sp.margin, sp.margin);

    const int cardW = 200;
    const int cardH = 120;
    const int x0 = sp.margin;
    const int y0 = sp.margin + 36;

    cardShow_ = Cards::create(workspace, x0, y0, cardW, cardH,
                              "Current Show", "—", StatusLevel::Inactive);
    cardRuntime_ = Cards::create(workspace, x0 + cardW + sp.gap, y0, cardW, cardH,
                                 "Runtime", "—", StatusLevel::Inactive);
    cardNetwork_ = Cards::create(workspace, x0 + 2 * (cardW + sp.gap), y0, cardW, cardH,
                                 "Network", "—", StatusLevel::Inactive);

    cardCue_ = Cards::create(workspace, x0, y0 + cardH + sp.gap, cardW, cardH,
                             "Cue", "—", StatusLevel::Inactive);
    cardDevices_ = Cards::create(workspace, x0 + cardW + sp.gap, y0 + cardH + sp.gap, cardW, cardH,
                                 "Devices", "—", StatusLevel::Inactive);
    cardSafety_ = Cards::create(workspace, x0 + 2 * (cardW + sp.gap), y0 + cardH + sp.gap, cardW, cardH,
                                "Safety", "CLEAR", StatusLevel::Healthy);

    events().subscribe(Event::ShowLoaded, &DashboardApp::onBus, this);
    events().subscribe(Event::ShowStarted, &DashboardApp::onBus, this);
    events().subscribe(Event::ShowStopped, &DashboardApp::onBus, this);
    events().subscribe(Event::ShowPaused, &DashboardApp::onBus, this);
    events().subscribe(Event::ShowFinished, &DashboardApp::onBus, this);
    events().subscribe(Event::CueChanged, &DashboardApp::onBus, this);
    events().subscribe(Event::ShowProgress, &DashboardApp::onBus, this);
    events().subscribe(Event::EmergencyChanged, &DashboardApp::onBus, this);
    events().subscribe(Event::LinkChanged, &DashboardApp::onBus, this);
    events().subscribe(Event::NodeJoined, &DashboardApp::onBus, this);
    events().subscribe(Event::NodeLost, &DashboardApp::onBus, this);
    events().subscribe(Event::NetworkHealth, &DashboardApp::onBus, this);
    events().subscribe(Event::DeviceChanged, &DashboardApp::onBus, this);
    events().subscribe(Event::SystemStatus, &DashboardApp::onBus, this);

    refreshAll();
  }

  void onDestroy() override {
    events().unsubscribeAll(this);
    root_ = nullptr;
    cardShow_ = cardRuntime_ = cardNetwork_ = {};
    cardCue_ = cardDevices_ = cardSafety_ = {};
  }

 private:
  lv_obj_t *root_ = nullptr;
  Cards::Handle cardShow_{};
  Cards::Handle cardRuntime_{};
  Cards::Handle cardNetwork_{};
  Cards::Handle cardCue_{};
  Cards::Handle cardDevices_{};
  Cards::Handle cardSafety_{};

  static void onBus(const EventPayload &ev, void *user) {
    DashboardApp *self = static_cast<DashboardApp *>(user);
    if (!self || !self->root_) return;
    self->onEvent(ev);
  }

  void onEvent(const EventPayload &ev) {
    switch (ev.type) {
      case Event::ShowLoaded:
      case Event::ShowStarted:
      case Event::ShowStopped:
      case Event::ShowPaused:
      case Event::ShowFinished:
      case Event::ShowProgress:
      case Event::SystemStatus:
        refreshShowCards();
        break;
      case Event::CueChanged:
        refreshCueCard();
        refreshShowCards();
        break;
      case Event::EmergencyChanged:
        refreshSafetyCard();
        refreshShowCards();
        break;
      case Event::LinkChanged:
      case Event::NodeJoined:
      case Event::NodeLost:
      case Event::NetworkHealth:
        refreshNetworkCard();
        break;
      case Event::DeviceChanged:
        refreshDevicesCard();
        break;
      default:
        break;
    }
  }

  void refreshAll() {
    refreshShowCards();
    refreshCueCard();
    refreshNetworkCard();
    refreshDevicesCard();
    refreshSafetyCard();
  }

  void refreshShowCards() {
    ShowService *show = services().get<ShowService>("show");
    if (!show) return;
    Cards::update(cardShow_, show->title(), show->status());
    char runtimeLine[48];
    snprintf(runtimeLine, sizeof(runtimeLine), "%s · %u%%",
             show->playbackLabel(), (unsigned)show->progressPercent());
    Cards::update(cardRuntime_, runtimeLine, show->status());
  }

  void refreshCueCard() {
    ShowService *show = services().get<ShowService>("show");
    if (!show) return;
    char cueLine[32];
    show->formatCue(cueLine, sizeof(cueLine));
    StatusLevel cueSt = show->loaded() ? StatusLevel::Healthy : StatusLevel::Inactive;
    if (show->running()) cueSt = StatusLevel::Working;
    Cards::update(cardCue_, cueLine, cueSt);
  }

  void refreshNetworkCard() {
    NetworkService *net = services().get<NetworkService>("network");
    if (!net) return;
    char netLine[40];
    char nodes[16];
    net->formatNodes(nodes, sizeof(nodes));
    snprintf(netLine, sizeof(netLine), "%s · %s", net->linkLabel(), nodes);
    Cards::update(cardNetwork_, netLine, net->status());
  }

  void refreshDevicesCard() {
    DeviceService *dev = services().get<DeviceService>("device");
    if (!dev) return;
    char devLine[32];
    snprintf(devLine, sizeof(devLine), "%d present", dev->presentCount());
    Cards::update(cardDevices_, devLine, dev->status());
  }

  void refreshSafetyCard() {
    ShowService *show = services().get<ShowService>("show");
    if (!show) return;
    if (show->emergency()) {
      Cards::update(cardSafety_, "LOCKED", StatusLevel::Critical);
    } else {
      Cards::update(cardSafety_, "CLEAR", StatusLevel::Healthy);
    }
  }
};

}  // namespace Os2