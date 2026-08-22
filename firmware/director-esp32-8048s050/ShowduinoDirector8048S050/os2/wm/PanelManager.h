#pragma once

#include <lvgl.h>
#include "../OsTypes.h"
#include "../theme/Theme.h"

namespace Os2 {

/**
 * Layer 5 — Window Manager (panels, not windows).
 *
 * Dashboard → Lighting slides in → Fixture inspector from right → Back slides away.
 * Nothing reloads. Nothing flickers. Everything feels alive.
 */
class PanelManager {
 public:
  static constexpr int kMaxStack = 8;

  void bind(lv_obj_t *workspaceHost, lv_obj_t *inspectorHost, lv_obj_t *overlayHost) {
    workspaceHost_ = workspaceHost;
    inspectorHost_ = inspectorHost;
    overlayHost_ = overlayHost;
  }

  lv_obj_t *workspaceHost() const { return workspaceHost_; }
  lv_obj_t *inspectorHost() const { return inspectorHost_; }
  lv_obj_t *overlayHost() const { return overlayHost_; }

  lv_obj_t *resetWorkspace() {
    if (!workspaceHost_) return nullptr;
    lv_obj_clean(workspaceHost_);
    stackDepth_ = 0;
    hideInspector(false);
    return workspaceHost_;
  }

  lv_obj_t *openInspector() {
    if (!inspectorHost_) return nullptr;
    lv_obj_clean(inspectorHost_);
    lv_obj_clear_flag(inspectorHost_, LV_OBJ_FLAG_HIDDEN);

    const Theme::Spacing &sp = Theme::engine().space();
    const Theme::Animation &an = Theme::engine().anim();
    const int panelW = sp.workspaceW() / 2;

    lv_obj_set_width(inspectorHost_, panelW);
    lv_obj_set_x(inspectorHost_, sp.screenW);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, inspectorHost_);
    lv_anim_set_values(&a, sp.screenW, sp.screenW - panelW);
    lv_anim_set_time(&a, an.normalMs);
    lv_anim_set_exec_cb(&a, [](void *obj, int32_t v) {
      lv_obj_set_x(static_cast<lv_obj_t *>(obj), v);
    });
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);

    if (stackDepth_ < kMaxStack) {
      stack_[stackDepth_++] = PanelKind::Inspector;
    }
    return inspectorHost_;
  }

  void hideInspector(bool animate = true) {
    if (!inspectorHost_) return;
    if (!animate) {
      lv_obj_add_flag(inspectorHost_, LV_OBJ_FLAG_HIDDEN);
      return;
    }
    const Theme::Spacing &sp = Theme::engine().space();
    const Theme::Animation &an = Theme::engine().anim();
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, inspectorHost_);
    lv_anim_set_values(&a, lv_obj_get_x(inspectorHost_), sp.screenW);
    lv_anim_set_time(&a, an.normalMs);
    lv_anim_set_exec_cb(&a, [](void *obj, int32_t v) {
      lv_obj_set_x(static_cast<lv_obj_t *>(obj), v);
    });
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_set_completed_cb(&a, [](lv_anim_t *anim) {
      lv_obj_add_flag(static_cast<lv_obj_t *>(anim->var), LV_OBJ_FLAG_HIDDEN);
    });
    lv_anim_start(&a);
    if (stackDepth_ > 0 && stack_[stackDepth_ - 1] == PanelKind::Inspector) {
      --stackDepth_;
    }
  }

  bool back() {
    if (stackDepth_ <= 0) return false;
    PanelKind top = stack_[--stackDepth_];
    if (top == PanelKind::Inspector) {
      hideInspector(true);
      return true;
    }
    return false;
  }

 private:
  lv_obj_t *workspaceHost_ = nullptr;
  lv_obj_t *inspectorHost_ = nullptr;
  lv_obj_t *overlayHost_ = nullptr;
  PanelKind stack_[kMaxStack]{};
  int stackDepth_ = 0;
};

}  // namespace Os2