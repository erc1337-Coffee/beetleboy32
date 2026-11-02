#pragma once

#include "ui/screen.h"

// Settings > zzz: how long without a touch before the screen goes off ("Screen off"), and before
// the device sleeps with Wi-Fi off until the next cooldown ("Sleep"). Each value opens a few fixed
// choices as squares; picking one saves it.
class SleepScreen : public Screen {
 public:
  explicit SleepScreen(App &app) : Screen(app) {}
  const char *name() const override { return "sleep"; }

 protected:
  void build(lv_obj_t *root) override;

 private:
  void showValues();
  void openChoices(int row);  // 0: Screen off, 1: Sleep
  static void onValue(lv_event_t *e);
  static void onChoice(lv_event_t *e);

  lv_obj_t *values_[2] = {};  // the value buttons' labels
  lv_obj_t *veil_ = nullptr;  // the open list, if any
  int row_ = 0;               // the list's row
};
