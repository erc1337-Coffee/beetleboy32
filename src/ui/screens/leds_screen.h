#pragma once

#include <stddef.h>

#include <vector>

#include "media/lights.h"
#include "ui/screen.h"

// Demos > LEDs: every light effect (media/lights.h) in a list; tapping one runs it (again: stops it),
// over whatever the lights were showing until the screen closes.
class LedsScreen : public Screen {
 public:
  explicit LedsScreen(App &app) : Screen(app) {}
  ~LedsScreen() override;
  const char *name() const override { return "leds"; }

 protected:
  void build(lv_obj_t *root) override;

 private:
  void show();  // the labels, and the lights
  static void onRow(lv_event_t *e);

  LightEffect effect_ = LightEffect::Count;  // Count: none
  size_t level_ = 1;  // in LEVELS
  std::vector<lv_obj_t *> labels_;
};
