#pragma once
// Only satisfy the unrelated inline UI-motion header; no UI rendering is tested.
struct lv_obj_t {};
enum {LV_SCR_LOAD_ANIM_FADE_ON};
inline lv_obj_t* lv_screen_active(){return nullptr;}
inline void lv_screen_load(lv_obj_t*){}
inline bool lv_obj_is_valid(lv_obj_t*){return false;}
inline void lv_obj_delete(lv_obj_t*){}
inline void lv_screen_load_anim(lv_obj_t*,int,unsigned,unsigned,bool){}
