#include "ui/screens/time_screen.h"

#include <math.h>
#include <stdio.h>
#include <time.h>

#include <string>

#include "app/app.h"
#include "assets/assets.h"
#include "board_config.h"
#include "hal/hal.h"
#include "ui/theme.h"
#include "ui/widgets.h"

// The map: world_map.png (made by scripts/world_map.py).
static constexpr int32_t MAP_W = 200, MAP_H = 100, MAP_Y = BELOW_BACK;
static constexpr int32_t MAP_X = (SCREEN_W - MAP_W) / 2;
// Hour : minute, AM/PM, as big as fits: a little wider than the map, down to the bottom edge.
static constexpr int32_t ROW_X = MAP_X - 8, ROW_Y = MAP_Y + MAP_H + 6;
static constexpr int32_t ROLLER_W = 64, COLON_GAP = 14, GAP = 10, VISIBLE_ROWS = 3, ROW_SPACE = 8;
static constexpr uint32_t ROLLER_TEXT = 0x8a8d96, ROLLER_BG = 0xf5f6f9, ROLLER_BORDER = 0xe2e3e8;
static constexpr uint32_t MAP_BORDER = 0xcfd2d9;
static constexpr float DEG = 3.14159265f / 180;
// Night over the (transparent) sea: a light dusk tint, so day and night read on water too.
static constexpr uint8_t NIGHT_SEA_R = 0x5a, NIGHT_SEA_G = 0x60, NIGHT_SEA_B = 0x78, NIGHT_SEA_OPA = 90;

// "00\n01\n...": LVGL rollers take their options as one newline-separated string. Index i shows
// i, except 0, which shows `zero_as` if given (12 on a 12-hour clock).
static std::string numbers(int count, int zero_as = 0) {
  std::string options;
  char buf[4];
  for (int i = 0; i < count; i++) {
    snprintf(buf, sizeof(buf), i ? "\n%02d" : "%02d", i ? i : zero_as);
    options += buf;
  }
  return options;
}

static lv_obj_t *roller(lv_obj_t *parent, const std::string &options, lv_roller_mode_t mode, int selected) {
  lv_obj_t *r = lv_roller_create(parent);
  lv_roller_set_options(r, options.c_str(), mode);
  lv_roller_set_visible_row_count(r, VISIBLE_ROWS);
  lv_roller_set_selected(r, selected, LV_ANIM_OFF);
  lv_obj_set_width(r, ROLLER_W);
  lv_obj_set_style_text_font(r, theme::font_title(), 0);
  lv_obj_set_style_text_line_space(r, ROW_SPACE, 0);
  lv_obj_set_style_text_color(r, lv_color_hex(ROLLER_TEXT), 0);
  lv_obj_set_style_bg_color(r, lv_color_hex(ROLLER_BG), 0);
  lv_obj_set_style_border_color(r, lv_color_hex(ROLLER_BORDER), 0);
  lv_obj_set_style_radius(r, 12, 0);
  lv_obj_set_style_bg_color(r, lv_color_hex(theme::INK), LV_PART_SELECTED);
  lv_obj_set_style_text_color(r, lv_color_white(), LV_PART_SELECTED);
  return r;
}

// Minutes east of UTC in the current timezone (what hal_local_time() applies).
static int utc_offset_minutes() {
  time_t t = time(nullptr);
  struct tm local, utc;
  localtime_r(&t, &local);
  gmtime_r(&t, &utc);
  int d = (local.tm_hour * 60 + local.tm_min) - (utc.tm_hour * 60 + utc.tm_min);
  if (d > 14 * 60) d -= 24 * 60;  // the two straddle midnight
  if (d < -12 * 60) d += 24 * 60;
  return d;
}

void TimeScreen::build(lv_obj_t *root) {
  theme::style_light_screen(root);

  lv_obj_t *frame = lv_obj_create(root);
  lv_obj_remove_style_all(frame);
  lv_obj_set_size(frame, MAP_W, MAP_H);
  lv_obj_set_pos(frame, MAP_X, MAP_Y);
  lv_obj_set_style_radius(frame, 8, 0);
  lv_obj_set_style_clip_corner(frame, true, 0);
  lv_obj_remove_flag(frame, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_border_width(frame, 1, 0);  // the sea is see-through: outline the map
  lv_obj_set_style_border_color(frame, lv_color_hex(MAP_BORDER), 0);
  pixels_.assign((size_t)MAP_W * MAP_H * 3, 0);  // RGB565, then the alpha plane
  canvas_ = lv_canvas_create(frame);
  lv_canvas_set_buffer(canvas_, pixels_.data(), MAP_W, MAP_H, LV_COLOR_FORMAT_RGB565A8);

  struct tm now = {};
  hal_local_time(&now);  // midnight if the clock isn't set yet
  hour_ = roller(root, numbers(12, 12), LV_ROLLER_MODE_INFINITE, now.tm_hour % 12);
  lv_obj_set_pos(hour_, ROW_X, ROW_Y);
  lv_obj_add_event_cb(hour_, onPick, LV_EVENT_VALUE_CHANGED, this);
  minute_ = roller(root, numbers(60), LV_ROLLER_MODE_INFINITE, now.tm_min);
  lv_obj_set_pos(minute_, ROW_X + ROLLER_W + COLON_GAP, ROW_Y);
  lv_obj_add_event_cb(minute_, onPick, LV_EVENT_VALUE_CHANGED, this);
  pm_ = roller(root, "AM\nPM", LV_ROLLER_MODE_NORMAL, now.tm_hour >= 12);
  lv_obj_set_pos(pm_, ROW_X + 2 * ROLLER_W + COLON_GAP + GAP, ROW_Y);
  lv_obj_add_event_cb(pm_, onPick, LV_EVENT_VALUE_CHANGED, this);
  lv_obj_t *colon = ui_label(root, theme::font_title(), lv_color_hex(theme::INK), ":");
  lv_obj_align_to(colon, hour_, LV_ALIGN_OUT_RIGHT_MID, (COLON_GAP - 6) / 2, 0);  // ~6 px wide
  changed_ = false;

  addBackButton(root);
  showDaylight();
}

int TimeScreen::pickedHour() const {
  return (int)lv_roller_get_selected(hour_) + (lv_roller_get_selected(pm_) ? 12 : 0);  // 12 AM = 0
}

// Shades the map for the picked time (read in the current timezone): land as drawn where the sun is
// up, darker where it's down, a soft band at twilight; the sea stays clear by day, tinted at night.
// The sun's position ignores the equation of time (up to ~16 minutes off), which this map can't
// show anyway.
void TimeScreen::showDaylight() {
  int local = pickedHour() * 60 + (int)lv_roller_get_selected(minute_);
  float utc_hours = (((local - utc_offset_minutes()) % 1440 + 1440) % 1440) / 60.0f;
  struct tm today = {};
  hal_local_time(&today);
  float declination = -23.44f * DEG * cosf(2 * 3.14159265f * (today.tm_yday + 10) / 365);
  float sun_lon = -15 * (utc_hours - 12);  // where it's noon

  float cos_dlon[MAP_W];
  for (int x = 0; x < MAP_W; x++) cos_dlon[x] = cosf((-180 + (x + 0.5f) * 360 / MAP_W - sun_lon) * DEG);
  const float sin_d = sinf(declination), cos_d = cosf(declination);

  // world_map.png is RGB565A8: the color plane, then an alpha plane (land coverage; sea = 0).
  const uint8_t *map = img_world_map.data;
  const size_t alpha_plane = (size_t)MAP_W * MAP_H * 2;
  for (int y = 0; y < MAP_H; y++) {
    float lat = (90 - (y + 0.5f) * 180 / MAP_H) * DEG;
    float a = sinf(lat) * sin_d, b = cosf(lat) * cos_d;
    for (int x = 0; x < MAP_W; x++) {
      float elevation = a + b * cos_dlon[x];  // sine of the sun's height above the horizon
      float light = elevation > 0.05f ? 1 : elevation < -0.1f ? 0 : (elevation + 0.1f) / 0.15f;
      size_t p = (size_t)y * MAP_W + x;
      uint16_t c = map[p * 2] | map[p * 2 + 1] << 8;
      float land = map[alpha_plane + p] / 255.0f;

      // Land at night: darker, a little bluer. In 8-bit for blending.
      float k_rg = 0.55f + 0.45f * light, k_b = 0.65f + 0.35f * light;
      float lr = (c >> 11) * 255 / 31.0f * k_rg, lg = (c >> 5 & 63) * 255 / 63.0f * k_rg,
            lb = (c & 31) * 255 / 31.0f * k_b;
      // Land over the night tint (which is clear in daylight).
      float sea = (1 - light) * NIGHT_SEA_OPA / 255.0f * (1 - land);
      float opa = land + sea;
      float r = 0, g = 0, bl = 0;
      if (opa > 0) {
        r = (lr * land + NIGHT_SEA_R * sea) / opa;
        g = (lg * land + NIGHT_SEA_G * sea) / opa;
        bl = (lb * land + NIGHT_SEA_B * sea) / opa;
      }
      uint16_t out = (uint16_t)((int)r >> 3 << 11 | (int)g >> 2 << 5 | (int)bl >> 3);
      pixels_[p * 2] = out & 0xff;
      pixels_[p * 2 + 1] = out >> 8;
      pixels_[alpha_plane + p] = (uint8_t)(opa * 255);
    }
  }
  lv_obj_invalidate(canvas_);
}

void TimeScreen::onPick(lv_event_t *e) {
  auto *self = static_cast<TimeScreen *>(lv_event_get_user_data(e));
  self->changed_ = true;
  self->showDaylight();
}

// Leaving saves what was picked (untouched wheels change nothing).
bool TimeScreen::onInput(const InputEvent &event) {
  if (event.action == InputAction::Back && changed_) {
    changed_ = false;
    app().setLocalTime(pickedHour(), (int)lv_roller_get_selected(minute_));
  }
  return false;  // Back still closes the screen
}
