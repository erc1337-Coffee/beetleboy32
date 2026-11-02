#pragma once

#include <functional>
#include <string>
#include <vector>

#include "ui/screen.h"

// A grid of whitish tiles (2 per row, scrolls past 4) with a Back button, and the battery top right
// (a lightning while USB is connected), the bell left of it. Tiles can be greyed out until they're usable.
// Opened on top of whatever is showing; Back returns to it.
class MenuScreen : public Screen {
 public:
  struct Item {
    std::string label;
    const void *icon;  // LVGL image (e.g. &img_menu_pfp_001); nullptr = placeholder
    std::function<void()> run;
    std::function<bool()> enabled;  // greyed out while it says no; nullptr: always on
  };

  MenuScreen(App &app, std::vector<Item> items, std::function<void()> onBell)
      : Screen(app), items_(std::move(items)), onBell_(std::move(onBell)) {}
  const char *name() const override { return "menu"; }
  void update() override;  // the battery

 protected:
  void build(lv_obj_t *root) override;

 private:
  static void onClick(lv_event_t *e);

  std::vector<Item> items_;  // not resized after build(): tiles hold pointers into it
  std::function<void()> onBell_;
  std::vector<lv_obj_t *> tiles_;  // as items_
  lv_obj_t *batteryIcon_ = nullptr, *batteryText_ = nullptr;
  int batteryShown_ = -2;  // percent on screen (-1: no reading)
  int usbShown_ = -1;      // 1: the lightning is up (USB), 0: the battery
};
