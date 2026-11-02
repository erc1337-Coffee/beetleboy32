#pragma once

#include <string>

#include "services/provisioning.h"
#include "ui/screen.h"

// Setup with the phone, step 1: a QR code that joins the device's own Wi-Fi network.
// The name and password are printed too, for joining from a laptop.
class SetupJoinScreen : public Screen {
 public:
  SetupJoinScreen(App &app, SetupNetwork network) : Screen(app), network_(std::move(network)) {}
  const char *name() const override { return "setup_join"; }

 protected:
  void build(lv_obj_t *root) override;

 private:
  SetupNetwork network_;
};
