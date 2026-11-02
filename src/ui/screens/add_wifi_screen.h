#pragma once

#include <vector>

#include "services/wifi_station.h"
#include "ui/screen.h"

// Settings > Wi-Fi > Add: the networks around (scanned on opening), strongest first, with their
// signal; open ones say so. Tapping one joins it, after the keyboard if it needs a password.
class AddWifiScreen : public Screen {
 public:
  explicit AddWifiScreen(App &app) : Screen(app) {}
  ~AddWifiScreen() override;
  const char *name() const override { return "add_wifi"; }
  void update() override;  // the list once the scan is done

 protected:
  void build(lv_obj_t *root) override;

 private:
  void scan();
  void fill();
  static void onPick(lv_event_t *e);
  static void onScan(lv_event_t *e);
  static void rescan(void *self);  // lv_async_call target: scan()

  lv_obj_t *list_ = nullptr;
  bool waiting_ = false;  // for the scan
  std::vector<WifiStation::ScanResult> networks_;
};
