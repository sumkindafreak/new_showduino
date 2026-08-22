#pragma once

#include <stdio.h>
#include "App.h"
#include "../theme/Theme.h"
#include "../services/StubServices.h"
#include "../services/AssetService.h"
#include "../services/SessionService.h"
#include "../services/CommandService.h"
#include "../models/Production.h"
#include "../events/EventBus.h"
#include "../widgets/Card.h"

namespace Os2 {

/**
 * Library.app — production media browser.
 *
 * Browse · Search · Filter · Preview · Load
 * Never opens files. Never talks to Stage.
 * Catalogue from AssetService; load via ShowService.load(id).
 * Runtime monitoring stays on Dashboard.
 */
class LibraryApp : public IApp {
 public:
  AppId appId() const override { return AppId::Library; }
  const char *id() const override { return "library"; }
  const char *title() const override { return "Library"; }
  const char *dockGlyph() const override { return "L"; }
  bool showInDock() const override { return true; }

  void onCreate(lv_obj_t *workspace) override {
    root_ = workspace;
    Theme::Engine &th = Theme::engine();
    const Theme::Spacing &sp = th.space();

    lv_obj_t *heading = lv_label_create(workspace);
    lv_label_set_text(heading, "Library");
    lv_obj_add_style(heading, &th.styleTitle, 0);
    lv_obj_set_pos(heading, sp.margin, sp.margin);

    hint_ = lv_label_create(workspace);
    lv_label_set_text(hint_, "Productions");
    lv_obj_add_style(hint_, &th.styleCaption, 0);
    lv_obj_set_pos(hint_, sp.margin, sp.margin + 28);

    list_ = lv_obj_create(workspace);
    lv_obj_remove_style_all(list_);
    lv_obj_set_pos(list_, sp.margin, sp.margin + 52);
    lv_obj_set_size(list_, sp.workspaceW() - sp.margin * 2 - 220, sp.workspaceH() - 64);
    lv_obj_set_style_bg_opa(list_, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(list_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(list_, LV_DIR_VER);

    detail_ = lv_obj_create(workspace);
    lv_obj_remove_style_all(detail_);
    lv_obj_add_style(detail_, &th.styleCard, 0);
    lv_obj_set_pos(detail_, sp.workspaceW() - 208 - sp.margin, sp.margin + 52);
    lv_obj_set_size(detail_, 208, sp.workspaceH() - 64);
    lv_obj_clear_flag(detail_, LV_OBJ_FLAG_SCROLLABLE);

    detailTitle_ = lv_label_create(detail_);
    lv_label_set_text(detailTitle_, "Select a production");
    lv_obj_add_style(detailTitle_, &th.styleBody, 0);
    lv_obj_set_pos(detailTitle_, sp.pad, sp.pad);
    lv_label_set_long_mode(detailTitle_, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(detailTitle_, 180);

    detailMeta_ = lv_label_create(detail_);
    lv_label_set_text(detailMeta_, "");
    lv_obj_add_style(detailMeta_, &th.styleCaption, 0);
    lv_obj_set_pos(detailMeta_, sp.pad, sp.pad + 40);
    lv_label_set_long_mode(detailMeta_, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(detailMeta_, 180);

    loadBtn_ = lv_button_create(detail_);
    lv_obj_remove_style_all(loadBtn_);
    lv_obj_add_style(loadBtn_, &th.styleButton, 0);
    lv_obj_add_style(loadBtn_, &th.styleButtonPressed, LV_STATE_PRESSED);
    lv_obj_set_size(loadBtn_, 180, 44);
    lv_obj_align(loadBtn_, LV_ALIGN_BOTTOM_MID, 0, -sp.pad);
    lv_obj_add_state(loadBtn_, LV_STATE_DISABLED);
    lv_obj_add_event_cb(loadBtn_, onLoadClicked, LV_EVENT_CLICKED, this);
    lv_obj_t *loadLab = lv_label_create(loadBtn_);
    lv_label_set_text(loadLab, "Load");
    lv_obj_center(loadLab);

    events().subscribe(Event::CatalogueChanged, &LibraryApp::onBus, this);
    events().subscribe(Event::ProductionSelected, &LibraryApp::onBus, this);
    events().subscribe(Event::ShowLoaded, &LibraryApp::onBus, this);

    rebuildList();
    refreshDetail();
  }

  void onBuildContext(lv_obj_t *strip) override {
    Theme::Engine &th = Theme::engine();
    lv_obj_t *lab = lv_label_create(strip);
    lv_label_set_text(lab, "Browse  ·  Load  ·  Dashboard for runtime");
    lv_obj_add_style(lab, &th.styleCaption, 0);
    lv_obj_align(lab, LV_ALIGN_LEFT_MID, 8, 0);
  }

  bool onSearch(const char *query) override {
    AssetService *assets = services().get<AssetService>("asset");
    if (!assets || !query) return false;
    const ProductionManifest *hits[8];
    int n = assets->search(query, hits, 8);
    if (n <= 0) return false;
    assets->select(hits[0]->id);
    /* Caller (future universal search) opens Library; we highlight best match. */
    return true;
  }

  void onDestroy() override {
    events().unsubscribeAll(this);
    root_ = list_ = detail_ = nullptr;
    detailTitle_ = detailMeta_ = loadBtn_ = hint_ = nullptr;
    rowCount_ = 0;
  }

 private:
  lv_obj_t *root_ = nullptr;
  lv_obj_t *list_ = nullptr;
  lv_obj_t *detail_ = nullptr;
  lv_obj_t *detailTitle_ = nullptr;
  lv_obj_t *detailMeta_ = nullptr;
  lv_obj_t *loadBtn_ = nullptr;
  lv_obj_t *hint_ = nullptr;
  lv_obj_t *rows_[AssetService::kMaxProductions]{};
  int rowCount_ = 0;

  static void onBus(const EventPayload &ev, void *user) {
    LibraryApp *self = static_cast<LibraryApp *>(user);
    if (!self || !self->root_) return;
    if (ev.type == Event::CatalogueChanged) {
      self->rebuildList();
    } else if (ev.type == Event::ProductionSelected) {
      self->rebuildList(); /* selection chrome */
      self->refreshDetail();
      return;
    }
    self->refreshDetail();
  }

  static void onRowClicked(lv_event_t *e) {
    LibraryApp *self = static_cast<LibraryApp *>(lv_event_get_user_data(e));
    lv_obj_t *row = (lv_obj_t *)lv_event_get_target(e);
    const char *id = static_cast<const char *>(lv_obj_get_user_data(row));
    if (!self || !id) return;
    AssetService *assets = services().get<AssetService>("asset");
    if (assets) {
      /* Selection is intent — goes through CommandService for history/macros later. */
      commandService().selectProduction(id);
    }
  }

  static void onLoadClicked(lv_event_t *e) {
    LibraryApp *self = static_cast<LibraryApp *>(lv_event_get_user_data(e));
    if (!self) return;
    AssetService *assets = services().get<AssetService>("asset");
    SessionService *session = services().get<SessionService>("session");
    if (!assets) return;
    const ProductionManifest *p = assets->selected();
    if (!p) return;
    const char *entry = p->entryShow[0] ? p->entryShow : p->id;
    /* Intent via CommandService — never touch ShowService for actions. */
    if (commandService().loadProduction(entry)) {
      if (session) session->setLastProduction(p->id);
    }
  }

  void rebuildList() {
    if (!list_) return;
    Theme::Engine &th = Theme::engine();
    const Theme::Colors &c = th.colors();
    const Theme::Spacing &sp = th.space();
    AssetService *assets = services().get<AssetService>("asset");

    lv_obj_clean(list_);
    rowCount_ = 0;

    if (!assets || assets->count() == 0) {
      lv_label_set_text(hint_, "No productions");
      lv_obj_t *empty = lv_label_create(list_);
      lv_label_set_text(empty, "Catalogue is empty");
      lv_obj_add_style(empty, &th.styleCaption, 0);
      return;
    }

    char hintBuf[32];
    snprintf(hintBuf, sizeof(hintBuf), "%d productions", assets->count());
    lv_label_set_text(hint_, hintBuf);

    const char *sel = assets->selectedId();
    int y = 0;
    for (int i = 0; i < assets->count(); ++i) {
      const ProductionManifest *p = assets->at(i);
      if (!p) continue;

      lv_obj_t *row = lv_obj_create(list_);
      lv_obj_remove_style_all(row);
      lv_obj_add_style(row, &th.styleSurfaceRaised, 0);
      lv_obj_set_size(row, lv_pct(100), 64);
      lv_obj_set_pos(row, 0, y);
      lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_user_data(row, (void *)p->id);
      lv_obj_add_event_cb(row, onRowClicked, LV_EVENT_CLICKED, this);

      bool on = sel && sel[0] && strcmp(sel, p->id) == 0;
      lv_obj_set_style_border_color(row, lv_color_hex(on ? c.accent : c.outline), 0);
      lv_obj_set_style_border_width(row, on ? 2 : 1, 0);

      lv_obj_t *name = lv_label_create(row);
      lv_label_set_text(name, p->name[0] ? p->name : p->id);
      lv_obj_add_style(name, &th.styleBody, 0);
      lv_obj_set_pos(name, sp.pad, 10);

      char sub[48];
      char dur[24];
      productionFormatDuration(*p, dur, sizeof(dur));
      snprintf(sub, sizeof(sub), "%s · %s",
               productionReadinessLabel(p->readiness), dur);
      lv_obj_t *meta = lv_label_create(row);
      lv_label_set_text(meta, sub);
      lv_obj_add_style(meta, &th.styleCaption, 0);
      lv_obj_set_style_text_color(meta, lv_color_hex(c.status(p->readiness)), 0);
      lv_obj_set_pos(meta, sp.pad, 34);

      if (rowCount_ < AssetService::kMaxProductions) rows_[rowCount_++] = row;
      y += 72;
    }
  }

  void refreshDetail() {
    if (!detailTitle_ || !detailMeta_ || !loadBtn_) return;
    AssetService *assets = services().get<AssetService>("asset");
    Theme::Engine &th = Theme::engine();
    const Theme::Colors &c = th.colors();

    const ProductionManifest *p = assets ? assets->selected() : nullptr;
    if (!p) {
      lv_label_set_text(detailTitle_, "Select a production");
      lv_label_set_text(detailMeta_, "Browse the catalogue.\nLoad sends it to the Stage.\nMonitor on Dashboard.");
      lv_obj_add_state(loadBtn_, LV_STATE_DISABLED);
      return;
    }

    lv_label_set_text(detailTitle_, p->name[0] ? p->name : p->id);

    char dur[24];
    productionFormatDuration(*p, dur, sizeof(dur));
    char meta[256];
    snprintf(meta, sizeof(meta),
             "Status\n%s\n\nDuration\n%s\n\nScenes\n%u\n\nAudio\n%s\n\nLighting\n%s\n\nEffects\n%s\n\nVersion\n%s\n\nEdited\n%s",
             productionReadinessLabel(p->readiness),
             dur,
             (unsigned)p->capabilities.sceneCount,
             p->capabilities.audio ? "Yes" : "No",
             p->capabilities.lighting ? "Yes" : "No",
             p->capabilities.effects ? "Yes" : "No",
             p->version[0] ? p->version : "—",
             p->lastEdited[0] ? p->lastEdited : "—");
    lv_label_set_text(detailMeta_, meta);
    lv_obj_set_style_text_color(detailMeta_, lv_color_hex(c.textMuted), 0);
    lv_obj_clear_state(loadBtn_, LV_STATE_DISABLED);
  }
};

}  // namespace Os2