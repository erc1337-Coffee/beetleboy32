#pragma once

#include <lvgl.h>

#include "input/controller.h"

class App;

// The Back button's bottom edge (see Screen::addBackButton): content under it starts here or lower.
constexpr int32_t BELOW_BACK = 34;

// One full-screen view, owned by ScreenManager. build() runs the first time it is shown.
// Each screen has its own LVGL group, so button focus never leaks between screens.
// Screens are views: they render state and forward user intents to App, which owns the flow.
class Screen {
 public:
  explicit Screen(App &app) : app_(app) {}
  virtual ~Screen();
  Screen(const Screen &) = delete;
  Screen &operator=(const Screen &) = delete;

  virtual const char *name() const = 0;
  virtual void onShow() {}  // became the top screen (first time, or after the one above closed)
  virtual void update() {}  // every loop while on top
  // Input the screen may react to (taps outside widgets, swipes, Back...). Return true if handled;
  // otherwise the app applies its default (Back closes this screen, Reset asks to factory reset).
  virtual bool onInput(const InputEvent &event) { return false; }

 protected:
  virtual void build(lv_obj_t *root) = 0;
  App &app() { return app_; }
  lv_obj_t *root() const { return root_; }  // the screen object build() got; nullptr while not built
  // "‹ Back" button in the top-left corner; sends the same Back event as the physical Cancel button.
  // Tile: whitish tile. Text: white text only, for screens whose picture is the background.
  enum class BackLook { Tile, Text };
  lv_obj_t *addBackButton(lv_obj_t *parent, BackLook look = BackLook::Tile);

 private:
  friend class ScreenManager;
  void create();
  void destroyView();  // build() runs again on the next show

  App &app_;
  lv_obj_t *root_ = nullptr;
  lv_group_t *group_ = nullptr;
};
