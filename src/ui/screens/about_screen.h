#pragma once

#include "ui/screen.h"

// Project name on the plaque, repo link, version, and the update check (placeholder for now).
// A long-press on the plaque opens the demos.
class AboutScreen : public Screen {
 public:
  explicit AboutScreen(App &app) : Screen(app) {}
  const char *name() const override { return "about"; }

 protected:
  void build(lv_obj_t *root) override;

 private:
  static void onCheckUpdates(lv_event_t *e);
};
