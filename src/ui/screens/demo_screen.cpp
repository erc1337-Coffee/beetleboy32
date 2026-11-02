#include "ui/screens/demo_screen.h"

#include <stdint.h>

#include <string>

#include "app/app.h"
#include "ui/theme.h"
#include "ui/widgets.h"

static constexpr int32_t ROW_H = 48, NAME_W = UI_VHS_ROW_W;

// Add a demo: its name and the App method that opens it; a toggle also says whether it's on.
static const struct {
  const char *name;
  void (App::*open)();
  bool (App::*on)() const;
} DEMOS[] = {
    {"KEYBOARD", &App::openKeyboardDemo, nullptr},
    {"SOUNDS", &App::openSounds, nullptr},
    {"LEDS", &App::openLeds, nullptr},
    {"PERF OVERLAY", &App::togglePerfOverlay, &App::perfOverlay},
};
static constexpr size_t DEMO_COUNT = sizeof(DEMOS) / sizeof(DEMOS[0]);

void DemoScreen::build(lv_obj_t *root) {
  theme::style_blue_screen(root);
  lv_obj_t *list = ui_vhs_list(root, UI_VHS_LIST_TOP);
  labels_.clear();
  for (size_t i = 0; i < DEMO_COUNT; i++) {
    lv_obj_t *row = ui_vhs_list_row(list, ROW_H, onOpen, this);
    lv_obj_set_user_data(row, (void *)(intptr_t)i);
    labels_.push_back(ui_vhs_text(row, "", NAME_W));
    ui_vhs_one_line(labels_.back());
  }
  showLabels();
  addBackButton(root, BackLook::Text);
}

void DemoScreen::showLabels() {
  for (size_t i = 0; i < DEMO_COUNT; i++) {
    std::string text = DEMOS[i].name;
    if (DEMOS[i].on) text += (app().*DEMOS[i].on)() ? ": ON" : ": OFF";
    ui_vhs_set_text(labels_[i], text.c_str());
  }
}

void DemoScreen::onOpen(lv_event_t *e) {
  auto *self = static_cast<DemoScreen *>(lv_event_get_user_data(e));
  size_t i = (size_t)(intptr_t)lv_obj_get_user_data(static_cast<lv_obj_t *>(lv_event_get_current_target(e)));
  (self->app().*DEMOS[i].open)();
  self->showLabels();
}
