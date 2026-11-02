#pragma once

#include <string>

#include "ui/screen.h"

// Wi-Fi settings, on the blue screen: the network in use with its signal (live), then Known (the
// remembered networks) and Add (pick one around, type its password).
class WifiScreen : public Screen {
 public:
  explicit WifiScreen(App &app) : Screen(app) {}
  const char *name() const override { return "wifi"; }
  void update() override;  // the network and signal, once a second

 protected:
  void build(lv_obj_t *root) override;

 private:
  void refresh();  // reads the network and signal now
  static void onKnown(lv_event_t *e);
  static void onAdd(lv_event_t *e);

  lv_obj_t *bars_ = nullptr, *network_ = nullptr, *status_ = nullptr;
  uint32_t shownAt_ = 0;
};
