#include "ui/screens/settings_screen.h"

#include <math.h>

#include <vector>

#include "app/app.h"
#include "board_config.h"
#include "fonts/fonts.h"
#include "hal/hal.h"
#include "media/sounds.h"
#include "ui/theme.h"
#include "ui/widgets.h"

static constexpr int32_t PAD = 18;
static constexpr int32_t WIDTH = SCREEN_W - 2 * PAD;
// Theme row: the hue bar, then the light theme's square.
static constexpr int32_t BAR_Y = 50, BAR_H = 26, KNOB = 20;
static constexpr int32_t LIGHT = 30, LIGHT_GAP = 10, HUE_W = WIDTH - LIGHT - LIGHT_GAP;
static constexpr int32_t LIGHT_TOUCH = 8;  // extra touch margin around the square
static constexpr int32_t LEVEL_H = 14;  // brightness and volume sliders
static constexpr int32_t BRIGHTNESS_LABEL_Y = 92, BRIGHTNESS_Y = 112;
static constexpr int32_t VOLUME_LABEL_Y = 136, VOLUME_Y = 156;
// The bottom row: PFP, zzz, LED, Time, Wi-Fi; the zzz is a narrow square.
static constexpr int32_t DEVICE_ROW_Y = 182, BUTTON_GAP = 8;
static constexpr int32_t NARROW_W = 44, BUTTON_H = 34;
static constexpr int32_t BUTTON_W = (WIDTH - (SettingsScreen::TILE_COUNT - 1) * BUTTON_GAP - NARROW_W) /
                                    (SettingsScreen::TILE_COUNT - 1);
static constexpr int32_t CLOCK_Y = 16;
static constexpr uint32_t DISABLED_TEXT = 0xa0a3ab;
// The sliders' knob, and how much bigger it gets while held.
static constexpr int32_t KNOB_SHADOW = 6, KNOB_GROW = 4;
static constexpr uint32_t KNOB_BORDER = 0x8a8d96;
// The brightness and volume sliders: a pale track filling dark from the left.
static constexpr uint32_t TRACK_BG = 0xf5f6f9, TRACK_BORDER = 0xd5d7dd;
static constexpr uint32_t FILL_LEFT = 0x5b5e66, FILL_RIGHT = 0xd9dbe0;
static constexpr uint32_t BAR_SHADOW = 0x9fa2ab;  // under the hue bar, like a tile's
static constexpr int32_t LIGHT_RADIUS = 4, LIGHT_CHOSEN_BORDER = 3;

// The rainbow bar, drawn once: a capsule with antialiased ends, a glossy top and a darker rim.
// Hue runs 0-360 between the knob's end positions (BAR_H / 2 in from each end), red past them.
static const lv_image_dsc_t *hue_bar_image() {
  static std::vector<uint8_t> pixels;
  static lv_image_dsc_t image;
  if (!pixels.empty()) return &image;

  const int w = HUE_W, h = BAR_H;
  const float r = h / 2.0f;
  pixels.resize(w * h * 3);  // RGB565 plane, then the alpha plane
  for (int x = 0; x < w; x++) {
    float px = x + 0.5f;
    float t = fminf(fmaxf((px - r) / (w - 2 * r), 0.0f), 1.0f);
    lv_color_t base = theme::hsl((int)lroundf(t * 360), 80, 66);
    for (int y = 0; y < h; y++) {
      float py = y + 0.5f;
      float edge = r - hypotf(px - fminf(fmaxf(px, r), w - r), py - r);  // distance inside the outline
      float v = py / h;
      float white = 0.35f * (1 - v) * (1 - v), black = 0.15f * v * v + (edge < 1.5f ? 0.25f : 0.0f);
      auto shade = [&](uint8_t c) { return (int)((c + (255 - c) * white) * (1 - black)); };
      int cr = shade(base.red), cg = shade(base.green), cb = shade(base.blue);
      uint16_t rgb565 = (uint16_t)((cr >> 3) << 11 | (cg >> 2) << 5 | cb >> 3);
      int i = y * w + x;
      pixels[i * 2] = rgb565 & 0xff;
      pixels[i * 2 + 1] = rgb565 >> 8;
      pixels[w * h * 2 + i] = (uint8_t)(fminf(fmaxf(edge + 0.5f, 0.0f), 1.0f) * 255);
    }
  }
  image.header.magic = LV_IMAGE_HEADER_MAGIC;
  image.header.cf = LV_COLOR_FORMAT_RGB565A8;
  image.header.w = w;
  image.header.h = h;
  image.header.stride = w * 2;
  image.data_size = pixels.size();
  image.data = pixels.data();
  return &image;
}

// White round knob, KNOB wide on a track `track_h` tall; a bit bigger while held.
static void style_knob(lv_obj_t *slider, int32_t track_h) {
  lv_obj_set_style_bg_color(slider, lv_color_white(), LV_PART_KNOB);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
  lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
  lv_obj_set_style_border_width(slider, 1, LV_PART_KNOB);
  lv_obj_set_style_border_color(slider, lv_color_hex(KNOB_BORDER), LV_PART_KNOB);
  lv_obj_set_style_shadow_width(slider, KNOB_SHADOW, LV_PART_KNOB);
  lv_obj_set_style_shadow_opa(slider, LV_OPA_30, LV_PART_KNOB);
  lv_obj_set_style_pad_all(slider, (KNOB - track_h) / 2, LV_PART_KNOB);  // knob = track height + 2 * pad
  lv_obj_set_style_pad_all(slider, (KNOB + KNOB_GROW - track_h) / 2, LV_PART_KNOB | LV_STATE_PRESSED);
}

static lv_obj_t *tile_button(lv_obj_t *parent, const char *text, lv_obj_t **label) {
  lv_obj_t *btn = lv_button_create(parent);
  lv_obj_remove_style_all(btn);
  theme::style_tile(btn);
  ui_squish(btn);
  lv_obj_set_style_text_color(btn, lv_color_hex(DISABLED_TEXT), LV_STATE_DISABLED);
  lv_obj_set_style_border_color(btn, lv_color_hex(theme::INK), LV_STATE_CHECKED);
  lv_obj_set_style_border_width(btn, 2, LV_STATE_CHECKED);
  lv_obj_set_size(btn, BUTTON_W, BUTTON_H);
  *label = lv_label_create(btn);
  lv_label_set_text(*label, text);
  lv_obj_center(*label);
  return btn;
}

static const struct {
  const char *label;
  void (App::*open)();
  bool (App::*ready)() const;  // greyed out until it says yes; nullptr: always
  bool narrow;                 // NARROW_W wide, its label a shape (a silhouette, as on the craft bench)
} TILES[] = {{"PFP", &App::openPfpPicker, &App::pfpsLoaded, false},  // needs the profile loaded fresh
             {SHAPE_ZZZ, &App::openSleepSettings, nullptr, true},
             {"LED", &App::openLightSettings, nullptr, false},
             {"Time", &App::openTimeSetter, nullptr, false},
             {"Wi-Fi", &App::openWifi, nullptr, false}};
static_assert(sizeof(TILES) / sizeof(TILES[0]) == SettingsScreen::TILE_COUNT, "one entry per tile");

// "CAPTION" on the left and its value on the right, muted, above a slider.
static lv_obj_t *caption_row(lv_obj_t *root, const char *caption, int32_t y) {
  lv_obj_t *label = ui_label(root, &font_menlo_14, lv_color_hex(theme::INK_MUTED), caption);
  lv_obj_set_pos(label, PAD, y);
  lv_obj_t *value = ui_label(root, &font_menlo_14, lv_color_hex(theme::INK_MUTED));
  lv_obj_align(value, LV_ALIGN_TOP_RIGHT, -PAD, y);
  return value;
}

// A pale track filling dark from the left: brightness, volume.
static lv_obj_t *level_slider(lv_obj_t *root, int32_t y, int min, int max, int value) {
  lv_obj_t *slider = lv_slider_create(root);
  lv_obj_remove_style_all(slider);
  lv_obj_set_pos(slider, PAD, y);
  lv_obj_set_size(slider, WIDTH, LEVEL_H);
  lv_slider_set_range(slider, min, max);
  lv_slider_set_value(slider, value, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(slider, lv_color_hex(TRACK_BG), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(slider, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(slider, lv_color_hex(TRACK_BORDER), LV_PART_MAIN);
  lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_bg_color(slider, lv_color_hex(FILL_LEFT), LV_PART_INDICATOR);
  lv_obj_set_style_bg_grad_color(slider, lv_color_hex(FILL_RIGHT), LV_PART_INDICATOR);
  lv_obj_set_style_bg_grad_dir(slider, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
  style_knob(slider, LEVEL_H);
  return slider;
}

void SettingsScreen::build(lv_obj_t *root) {
  theme::style_light_screen(root);
  choice_ = app().themeSetting();

  clock_ = ui_label(root, &font_menlo_16, lv_color_hex(theme::INK));  // filled in by update()
  lv_obj_align(clock_, LV_ALIGN_TOP_RIGHT, -PAD, CLOCK_Y);

  // Theme: a hue, or the light (white) theme. Sent to the account on leaving (onInput).
  slider_ = lv_slider_create(root);
  lv_obj_remove_style_all(slider_);
  lv_obj_set_pos(slider_, PAD, BAR_Y);
  lv_obj_set_size(slider_, HUE_W, BAR_H);
  lv_slider_set_range(slider_, 0, 360);
  lv_slider_set_value(slider_, choice_.hue, LV_ANIM_OFF);
  lv_obj_set_style_bg_image_src(slider_, hue_bar_image(), LV_PART_MAIN);
  lv_obj_set_style_radius(slider_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_pad_hor(slider_, BAR_H / 2, LV_PART_MAIN);  // knob travel, matches the image
  lv_obj_set_style_shadow_width(slider_, 8, LV_PART_MAIN);
  lv_obj_set_style_shadow_offset_y(slider_, 2, LV_PART_MAIN);
  lv_obj_set_style_shadow_color(slider_, lv_color_hex(BAR_SHADOW), LV_PART_MAIN);
  lv_obj_set_style_shadow_opa(slider_, LV_OPA_40, LV_PART_MAIN);
  style_knob(slider_, BAR_H);
  lv_obj_add_event_cb(slider_, onHue, LV_EVENT_VALUE_CHANGED, this);

  light_ = lv_button_create(root);  // white with a thin black edge; a thick one when chosen
  lv_obj_remove_style_all(light_);
  lv_obj_set_size(light_, LIGHT, LIGHT);
  lv_obj_set_pos(light_, PAD + WIDTH - LIGHT, BAR_Y + (BAR_H - LIGHT) / 2);
  lv_obj_set_style_bg_color(light_, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(light_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(light_, lv_color_hex(theme::INK), 0);
  lv_obj_set_style_border_width(light_, 1, 0);
  lv_obj_set_style_border_width(light_, LIGHT_CHOSEN_BORDER, LV_STATE_CHECKED);
  lv_obj_set_style_radius(light_, LIGHT_RADIUS, 0);
  lv_obj_set_ext_click_area(light_, LIGHT_TOUCH);
  ui_squish(light_);
  lv_obj_add_event_cb(light_, onLight, LV_EVENT_CLICKED, this);

  // Device settings, applied while dragging, saved to the config store on release.
  brightnessValue_ = caption_row(root, "BRIGHTNESS", BRIGHTNESS_LABEL_Y);
  brightness_ = level_slider(root, BRIGHTNESS_Y, DeviceSettings::MIN_BRIGHTNESS, 100, app().brightness());
  lv_obj_add_event_cb(brightness_, onBrightness, LV_EVENT_VALUE_CHANGED, this);
  lv_obj_add_event_cb(brightness_, onBrightness, LV_EVENT_RELEASED, this);
  showBrightness();
  volumeValue_ = caption_row(root, "VOLUME", VOLUME_LABEL_Y);
  volume_ = level_slider(root, VOLUME_Y, 0, 100, app().volume());
  lv_obj_add_event_cb(volume_, onVolume, LV_EVENT_VALUE_CHANGED, this);
  lv_obj_add_event_cb(volume_, onVolume, LV_EVENT_RELEASED, this);
  showVolume();

  // Bottom row: the PFP picker, the sleep times, the lights, the time setter, Wi-Fi.
  int32_t x = PAD;
  for (int i = 0; i < TILE_COUNT; i++) {
    lv_obj_t *label;
    lv_obj_t *tile = tile_button(root, TILES[i].label, &label);
    const bool narrow = TILES[i].narrow;
    lv_obj_set_pos(tile, x, DEVICE_ROW_Y);
    x += (narrow ? NARROW_W : BUTTON_W) + BUTTON_GAP;
    if (narrow) {
      lv_obj_set_width(tile, NARROW_W);
      lv_obj_set_style_text_font(label, &font_shapes_22, 0);
      lv_obj_set_style_text_color(label, lv_color_black(), 0);
      lv_obj_set_style_text_opa(label, theme::SHAPE_OPA, 0);
    }
    lv_obj_set_user_data(tile, (void *)(intptr_t)i);
    lv_obj_add_event_cb(tile, onTile, LV_EVENT_CLICKED, this);
    tiles_[i] = tile;
  }

  addBackButton(root);
  showChoice();
  update();
}

void SettingsScreen::onTile(lv_event_t *e) {
  auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(e));
  int i = (int)(intptr_t)lv_obj_get_user_data(static_cast<lv_obj_t *>(lv_event_get_current_target(e)));
  (self->app().*TILES[i].open)();
}

void SettingsScreen::showBrightness() { lv_label_set_text_fmt(brightnessValue_, "%d%%", app().brightness()); }

void SettingsScreen::onBrightness(lv_event_t *e) {
  auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(e));
  bool released = lv_event_get_code(e) == LV_EVENT_RELEASED;
  self->app().setBrightness((int)lv_slider_get_value(self->brightness_), released);
  self->showBrightness();
}

void SettingsScreen::showVolume() {
  if (app().volume() == 0) {
    lv_label_set_text(volumeValue_, "OFF");
  } else {
    lv_label_set_text_fmt(volumeValue_, "%d%%", app().volume());
  }
}

// On release, a click at the new level.
void SettingsScreen::onVolume(lv_event_t *e) {
  auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(e));
  bool released = lv_event_get_code(e) == LV_EVENT_RELEASED;
  self->app().setVolume((int)lv_slider_get_value(self->volume_), released);
  self->showVolume();
  if (released) play_sound(Sound::Click);
}

void SettingsScreen::showChoice() {
  lv_obj_set_state(light_, LV_STATE_CHECKED, !choice_.colored);
  // The knob fades while the hue isn't in use.
  lv_obj_set_style_bg_opa(slider_, choice_.colored ? LV_OPA_COVER : LV_OPA_40, LV_PART_KNOB);
}

void SettingsScreen::update() {
  for (int i = 0; i < TILE_COUNT; i++) {  // greyed out until what they need has loaded
    if (TILES[i].ready) ui_set_enabled(tiles_[i], (app().*TILES[i].ready)());
  }
  struct tm now;
  int minute_of_day = hal_local_time(&now) ? now.tm_hour * 60 + now.tm_min : -2;
  if (minute_of_day == timeShown_) return;
  timeShown_ = minute_of_day;
  if (minute_of_day < 0) {
    lv_label_set_text(clock_, "--:--");
  } else {
    lv_label_set_text_fmt(clock_, "%d:%02d %s", (now.tm_hour + 11) % 12 + 1, now.tm_min,
                          now.tm_hour < 12 ? "AM" : "PM");
  }
}

// Leaving: the theme goes to the account if it changed (like the time setter, no Save button).
bool SettingsScreen::onInput(const InputEvent &event) {
  if (event.action == InputAction::Back && !(choice_ == app().themeSetting())) app().saveTheme(choice_);
  return false;
}

void SettingsScreen::onHue(lv_event_t *e) {
  auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(e));
  self->choice_ = {true, (int)lv_slider_get_value(self->slider_)};
  self->showChoice();
}

void SettingsScreen::onLight(lv_event_t *e) {
  auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(e));
  self->choice_.colored = false;  // keeps the hue, as the website does
  self->showChoice();
}
