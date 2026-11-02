#include "ui/screens/leds_screen.h"

#include <stdint.h>

#include <string>

#include "app/app.h"
#include "ui/theme.h"
#include "ui/widgets.h"

static constexpr int32_t ROW_H = 44, NAME_W = UI_VHS_ROW_W;
// Brightness steps (percent). The LEDs run off the board's 3V3: full white on all four draws
// ~240 mA, which it may not supply cleanly. Starts at the second one.
static constexpr int LEVELS[] = {10, 25, 50, 100};
static constexpr size_t LEVEL_COUNT = sizeof(LEVELS) / sizeof(LEVELS[0]);
static constexpr int BRIGHTNESS_ROW = (int)LightEffect::Count;  // the row after the effects

void LedsScreen::build(lv_obj_t *root) {
  theme::style_blue_screen(root);
  lv_obj_t *list = ui_vhs_list(root, UI_VHS_LIST_TOP);
  labels_.clear();
  for (int i = 0; i <= BRIGHTNESS_ROW; i++) {
    lv_obj_t *row = ui_vhs_list_row(list, ROW_H, onRow, this);
    lv_obj_set_user_data(row, (void *)(intptr_t)i);
    labels_.push_back(ui_vhs_text(row, "", NAME_W));
    ui_vhs_one_line(labels_.back());
  }
  show();
  addBackButton(root, BackLook::Text);
}

LedsScreen::~LedsScreen() { app().lights().stopDemo(); }

void LedsScreen::show() {
  for (int i = 0; i < BRIGHTNESS_ROW; i++) {
    const std::string name = light_effect_name((LightEffect)i);
    ui_vhs_set_text(labels_[i], ((LightEffect)i == effect_ ? "> " + name : name).c_str());
  }
  ui_vhs_set_text(labels_[BRIGHTNESS_ROW], ("BRIGHTNESS: " + std::to_string(LEVELS[level_]) + "%").c_str());
  app().lights().startDemo(effect_, LEVELS[level_]);
}

void LedsScreen::onRow(lv_event_t *e) {
  auto *self = static_cast<LedsScreen *>(lv_event_get_user_data(e));
  int row = (int)(intptr_t)lv_obj_get_user_data(static_cast<lv_obj_t *>(lv_event_get_current_target(e)));
  if (row == BRIGHTNESS_ROW) {
    self->level_ = (self->level_ + 1) % LEVEL_COUNT;
  } else {
    self->effect_ = (LightEffect)row == self->effect_ ? LightEffect::Count : (LightEffect)row;
  }
  self->show();
}
