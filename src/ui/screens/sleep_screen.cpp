#include "ui/screens/sleep_screen.h"

#include <stdint.h>
#include <stdio.h>

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

// Fixed choices, seconds: short to long.
static constexpr int CHOICES = 5;
static const struct {
  const char *label;
  int choices[CHOICES];
} ROWS[] = {
    {"Screen off", {15, 30, 60, 2 * 60, 5 * 60}},           // the screen goes off ("light sleep")
    {"Sleep", {5 * 60, 15 * 60, 30 * 60, 3600, 2 * 3600}},  // Wi-Fi off too, until a cooldown ends
};

static std::string duration(int s) {
  char buf[16];
  if (s < 60) snprintf(buf, sizeof(buf), "%d s", s);
  else if (s < 3600) snprintf(buf, sizeof(buf), "%d min", s / 60);
  else snprintf(buf, sizeof(buf), "%d h", s / 3600);
  return buf;
}

void SleepScreen::build(lv_obj_t *root) {
  theme::style_light_screen(root);
  veil_ = nullptr;

  for (int row = 0; row < 2; row++) {
    const int32_t y = ROW_Y + row * ROW_GAP;
    lv_obj_t *caption = ui_label(root, &font_menlo_16, lv_color_hex(theme::INK));
    lv_label_set_text_fmt(caption, "%s:", ROWS[row].label);
    lv_obj_align(caption, LV_ALIGN_TOP_LEFT, PAD, y + CAPTION_DY);
    lv_obj_t *value = ui_value_button(root, VALUE_W, VALUE_H, /*arrow=*/true, &values_[row]);
    lv_obj_align(value, LV_ALIGN_TOP_RIGHT, -PAD, y);
    lv_obj_set_user_data(value, (void *)(intptr_t)row);
    lv_obj_add_event_cb(value, onValue, LV_EVENT_CLICKED, this);
  }

  showValues();
  addBackButton(root);
}

void SleepScreen::showValues() {
  lv_label_set_text(values_[0], duration(app().screenOffSeconds()).c_str());
  lv_label_set_text(values_[1], duration(app().sleepSeconds()).c_str());
}

void SleepScreen::openChoices(int row) {
  row_ = row;
  const int seconds = row == 0 ? app().screenOffSeconds() : app().sleepSeconds();
  std::vector<std::string> labels;
  int current = -1;
  for (int i = 0; i < CHOICES; i++) {
    labels.push_back(duration(ROWS[row].choices[i]));
    if (ROWS[row].choices[i] == seconds) current = i;
  }
  veil_ = ui_choice_veil(root(), labels, current, onChoice, this);
}

void SleepScreen::onValue(lv_event_t *e) {
  auto *self = static_cast<SleepScreen *>(lv_event_get_user_data(e));
  if (self->veil_) return;
  auto *value = static_cast<lv_obj_t *>(lv_event_get_current_target(e));
  self->openChoices((int)(intptr_t)lv_obj_get_user_data(value));
}

void SleepScreen::onChoice(lv_event_t *e) {
  auto *self = static_cast<SleepScreen *>(lv_event_get_user_data(e));
  const int i = (int)(intptr_t)lv_obj_get_user_data(static_cast<lv_obj_t *>(lv_event_get_current_target(e)));
  const int s = ROWS[self->row_].choices[i];
  if (self->row_ == 0) self->app().setScreenOffSeconds(s);
  else self->app().setSleepSeconds(s);
  lv_obj_delete_async(self->veil_);  // its own button is running this
  self->veil_ = nullptr;
  self->showValues();
}
