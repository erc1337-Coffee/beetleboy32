#pragma once

#include "ui/screen.h"

// Settings > LED: the lights on or off, and how bright the always-on green is (the effects stay at
// full brightness). Like the zzz screen: the brightness opens a few fixed choices as squares.
class LightSettingsScreen : public Screen {
 public:
  explicit LightSettingsScreen(App &app) : Screen(app) {}
  const char *name() const override { return "light_settings"; }

 protected:
  void build(lv_obj_t *root) override;

 private:
  void showValues();
  void openChoices();
  static void onToggle(lv_event_t *e);
  static void onLevel(lv_event_t *e);
  static void onChoice(lv_event_t *e);

  lv_obj_t *toggle_ = nullptr, *level_ = nullptr;  // the value buttons' labels
  lv_obj_t *levelButton_ = nullptr;
  lv_obj_t *veil_ = nullptr;  // the open list, if any
};
