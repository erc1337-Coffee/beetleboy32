#pragma once

#include <vector>

#include "ui/screen.h"

// Long-press on About's plaque: a list of demos to try things out (the keyboard, the sounds...).
class DemoScreen : public Screen {
 public:
  explicit DemoScreen(App &app) : Screen(app) {}
  const char *name() const override { return "demos"; }

 protected:
  void build(lv_obj_t *root) override;

 private:
  void showLabels();  // a toggle's ON / OFF
  static void onOpen(lv_event_t *e);

  std::vector<lv_obj_t *> labels_;
};
