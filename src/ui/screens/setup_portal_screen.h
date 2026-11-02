#pragma once

#include <string>

#include "ui/screen.h"

// Setup with the phone, step 2: shown once a device has joined. Points at the setup page in case
// the phone didn't open it by itself.
class SetupPortalScreen : public Screen {
 public:
  SetupPortalScreen(App &app, std::string url) : Screen(app), url_(std::move(url)) {}
  const char *name() const override { return "setup_portal"; }

 protected:
  void build(lv_obj_t *root) override;

 private:
  std::string url_;
};
