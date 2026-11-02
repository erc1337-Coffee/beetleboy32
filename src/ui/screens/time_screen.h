#pragma once

#include <vector>

#include "ui/screen.h"

// Set the local time: hour, minute and AM/PM rollers, saved on leaving. The clock itself stays on
// UTC (NTP, RTC); what's entered becomes the timezone (App::setLocalTime). Above them, a world map
// shades where it's day and night at the picked time, so rolling the hour moves the daylight.
class TimeScreen : public Screen {
 public:
  explicit TimeScreen(App &app) : Screen(app) {}
  const char *name() const override { return "time"; }
  bool onInput(const InputEvent &event) override;  // Back saves a changed time

 protected:
  void build(lv_obj_t *root) override;

 private:
  int pickedHour() const;  // 0-23
  void showDaylight();
  static void onPick(lv_event_t *e);

  lv_obj_t *hour_ = nullptr;
  lv_obj_t *minute_ = nullptr;
  lv_obj_t *pm_ = nullptr;
  bool changed_ = false;  // a wheel was turned
  lv_obj_t *canvas_ = nullptr;
  std::vector<uint8_t> pixels_;  // the canvas's RGB565 buffer
};
