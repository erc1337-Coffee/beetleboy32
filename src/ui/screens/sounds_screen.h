#pragma once

#include "ui/screen.h"

// Demos > Sounds: every sound effect (media/sounds.h) in a list; tapping one plays it.
class SoundsScreen : public Screen {
 public:
  explicit SoundsScreen(App &app) : Screen(app) {}
  const char *name() const override { return "sounds"; }

 protected:
  void build(lv_obj_t *root) override;

 private:
  static void onPlay(lv_event_t *e);
};
