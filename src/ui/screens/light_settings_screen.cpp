#include "ui/screens/light_settings_screen.h"

#include <stdint.h>

#include <string>
#include <vector>

#include "app/app.h"
#include "fonts/fonts.h"
#include "ui/theme.h"
#include "ui/widgets.h"

static constexpr int32_t PAD = 18;
static constexpr int32_t ROW_Y = 72, ROW_GAP = 60;
static constexpr int32_t VALUE_W = 112, VALUE_H = 40;
static constexpr int32_t CAPTION_DY = (VALUE_H - 18) / 2;  // the caption centered on its value button
static constexpr int LEVELS[] = {5, 10, 25, 50, 100};  // percent
static constexpr int LEVEL_COUNT = sizeof(LEVELS) / sizeof(LEVELS[0]);

static void caption(lv_obj_t *root, const char *text, int32_t y) {
  lv_obj_t *label = ui_label(root, &font_menlo_16, lv_color_hex(theme::INK), text);
  lv_obj_align(label, LV_ALIGN_TOP_LEFT, PAD, y + CAPTION_DY);
}

void LightSettingsScreen::build(lv_obj_t *root) {
  theme::style_light_screen(root);
  veil_ = nullptr;

  caption(root, "Lights:", ROW_Y);
  lv_obj_t *toggle = ui_value_button(root, VALUE_W, VALUE_H, /*arrow=*/false, &toggle_);
  lv_obj_align(toggle, LV_ALIGN_TOP_RIGHT, -PAD, ROW_Y);
  lv_obj_add_event_cb(toggle, onToggle, LV_EVENT_CLICKED, this);

  caption(root, "Brightness:", ROW_Y + ROW_GAP);
  levelButton_ = ui_value_button(root, VALUE_W, VALUE_H, /*arrow=*/true, &level_);
  lv_obj_align(levelButton_, LV_ALIGN_TOP_RIGHT, -PAD, ROW_Y + ROW_GAP);
  lv_obj_add_event_cb(levelButton_, onLevel, LV_EVENT_CLICKED, this);

  showValues();
  addBackButton(root);
}

// The brightness is greyed out with the lights off: there's nothing it changes.
void LightSettingsScreen::showValues() {
  const bool on = app().lightsOn();
  lv_label_set_text(toggle_, on ? "ON" : "OFF");
  lv_label_set_text_fmt(level_, "%d%%", app().lightsPercent());
  ui_set_enabled(levelButton_, on);
}

void LightSettingsScreen::openChoices() {
  std::vector<std::string> labels;
  int current = -1;
  for (int i = 0; i < LEVEL_COUNT; i++) {
    labels.push_back(std::to_string(LEVELS[i]) + "%");
    if (LEVELS[i] == app().lightsPercent()) current = i;
  }
  veil_ = ui_choice_veil(root(), labels, current, onChoice, this);
}

void LightSettingsScreen::onToggle(lv_event_t *e) {
  auto *self = static_cast<LightSettingsScreen *>(lv_event_get_user_data(e));
  if (self->veil_) return;
  self->app().setLightsOn(!self->app().lightsOn());
  self->showValues();
}

void LightSettingsScreen::onLevel(lv_event_t *e) {
  auto *self = static_cast<LightSettingsScreen *>(lv_event_get_user_data(e));
  if (!self->veil_) self->openChoices();
}

void LightSettingsScreen::onChoice(lv_event_t *e) {
  auto *self = static_cast<LightSettingsScreen *>(lv_event_get_user_data(e));
  const int i = (int)(intptr_t)lv_obj_get_user_data(static_cast<lv_obj_t *>(lv_event_get_current_target(e)));
  self->app().setLightsPercent(LEVELS[i]);
  lv_obj_delete_async(self->veil_);  // its own button is running this
  self->veil_ = nullptr;
  self->showValues();
}
