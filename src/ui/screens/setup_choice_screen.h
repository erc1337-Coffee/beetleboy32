#pragma once

#include "app/device_config.h"
#include "ui/screen.h"

// Setup's first step: with the phone (the device's own Wi-Fi network and setup page), or on the
// device itself (scan and keyboard). Either one comes back here with Back.
class SetupChoiceScreen : public Screen {
 public:
  SetupChoiceScreen(App &app, SetupMode mode) : Screen(app), mode_(mode) {}
  const char *name() const override { return "setup_choice"; }

 protected:
  void build(lv_obj_t *root) override;

 private:
  static void onPhone(lv_event_t *e);
  static void onDevice(lv_event_t *e);

  SetupMode mode_;
};
