#pragma once

#include "account/profile.h"
#include "ui/screen.h"

// The member card: name, avatar, level and beetle stats, tinted with the user's theme hue.
// Draws from a Profile owned by App, which outlives the screen.
class HomeScreen : public Screen {
 public:
  HomeScreen(App &app, Profile &profile) : Screen(app), profile_(profile) {}

  struct Stat {  // one stat row
    lv_obj_t *value;
    lv_obj_t *dot;  // ready: something to claim
    long shown;     // value on screen
  };

  const char *name() const override { return "home"; }
  bool onInput(const InputEvent &event) override;  // tap or OK opens the menu
  void onShow() override;  // re-reads the counters (throttled by App)
  void update() override;  // counters and ready dots: cheese to claim, a trap or hunt available

 protected:
  void build(lv_obj_t *root) override;

 private:
  Profile &profile_;
  Stat cheese_{};
  Stat beetles_{};
  lv_obj_t *level_ = nullptr;
  int levelShown_ = -1;
  lv_obj_t *avatarBox_ = nullptr;
  bool avatarShown_ = false;
};
