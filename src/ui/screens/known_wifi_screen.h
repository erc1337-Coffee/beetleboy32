#pragma once

#include "ui/screen.h"

// Settings > Wi-Fi > Known: the remembered networks, most recent first, the one in use with its
// signal. Each has a delete button (asks first), except the last one left.
class KnownWifiScreen : public Screen {
 public:
  explicit KnownWifiScreen(App &app) : Screen(app) {}
  const char *name() const override { return "known_wifi"; }
  void update() override;  // the list again once a network was forgotten

 protected:
  void build(lv_obj_t *root) override;

 private:
  void fill();
  static void onDelete(lv_event_t *e);

  lv_obj_t *list_ = nullptr;
  size_t shown_ = 0;  // networks in the list
};
