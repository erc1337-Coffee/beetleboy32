#include "ui/theme.h"

#include <math.h>

#include "assets/assets.h"

namespace theme {

// One style shared by every screen root, so changing the background restyles all of them at once.
static lv_style_t screen_style;

lv_color_t hsl(int h, int s, int l) {
  float sf = s / 100.0f, lf = l / 100.0f;
  float c = (1 - fabsf(2 * lf - 1)) * sf;
  float hp = fmodf((float)h, 360.0f) / 60.0f;
  float x = c * (1 - fabsf(fmodf(hp, 2.0f) - 1));
  float r = 0, g = 0, b = 0;
  if (hp < 1) r = c, g = x;
  else if (hp < 2) r = x, g = c;
  else if (hp < 3) g = c, b = x;
  else if (hp < 4) g = x, b = c;
  else if (hp < 5) r = x, b = c;
  else r = c, b = x;
  float m = lf - c / 2;
  return lv_color_make((uint8_t)((r + m) * 255 + 0.5f), (uint8_t)((g + m) * 255 + 0.5f),
                       (uint8_t)((b + m) * 255 + 0.5f));
}

// Same lightness per token for both: the default theme is the colored one without saturation.
static UserPalette palette(int hue, int saturation_pct) {
  hue = ((hue % 360) + 360) % 360;
  auto tone = [&](int s, int l) { return hsl(hue, s * saturation_pct / 100, l); };
  return {tone(80, 95), tone(60, 85), tone(50, 75), tone(35, 18), tone(60, 45)};
}

UserPalette user_palette(int hue) { return palette(hue, 100); }
UserPalette default_palette() { return palette(0, 0); }

void init() {
  lv_style_init(&screen_style);
  lv_style_set_bg_color(&screen_style, bg());
  lv_style_set_bg_opa(&screen_style, LV_OPA_COVER);
  lv_style_set_bg_image_recolor(&screen_style, lv_color_black());
  lv_style_set_bg_image_recolor_opa(&screen_style, BACKGROUND_DIM);
  lv_style_set_text_color(&screen_style, text());
  lv_style_set_text_font(&screen_style, font_body());
}

void style_textured(lv_obj_t *obj, lv_color_t color) {
  lv_obj_set_style_bg_color(obj, color, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  add_texture(obj);
}

void add_texture(lv_obj_t *obj, lv_opa_t opa) {
  // The website's texture: its 8x8 sample (assets/source/pattern_dots_8.png) repeated into a 64x64
  // tile, so a background is a few dozen draws instead of a thousand.
  lv_obj_set_style_bg_image_src(obj, &img_pattern_dots, 0);
  lv_obj_set_style_bg_image_opa(obj, opa, 0);
  lv_obj_set_style_bg_image_tiled(obj, true, 0);
  lv_obj_set_style_bg_image_recolor_opa(obj, LV_OPA_TRANSP, 0);  // screens dim their image; not this one
}

void style_tile(lv_obj_t *obj) {
  lv_obj_set_style_bg_color(obj, lv_color_hex(0xf5f6f9), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(obj, lv_color_hex(0xe4e6eb), LV_STATE_PRESSED);
  lv_obj_set_style_radius(obj, 12, 0);
  lv_obj_set_style_border_width(obj, 1, 0);
  lv_obj_set_style_border_color(obj, lv_color_hex(0xe2e3e8), 0);
  lv_obj_set_style_shadow_width(obj, 8, 0);
  lv_obj_set_style_shadow_offset_y(obj, 2, 0);
  lv_obj_set_style_shadow_color(obj, lv_color_hex(0x9fa2ab), 0);
  lv_obj_set_style_shadow_opa(obj, LV_OPA_30, 0);
  lv_obj_set_style_outline_width(obj, 2, LV_STATE_FOCUS_KEY);
  lv_obj_set_style_outline_color(obj, accent(), LV_STATE_FOCUS_KEY);
  lv_obj_set_style_text_color(obj, lv_color_hex(INK), 0);
}

void style_screen(lv_obj_t *root) { lv_obj_add_style(root, &screen_style, 0); }

void style_light_screen(lv_obj_t *root) { style_textured(root, lv_color_hex(LIGHT_BG)); }

void style_blue_screen(lv_obj_t *root) {
  lv_obj_set_style_bg_image_src(root, &img_rwo_blue_screen, 0);
  lv_obj_set_style_bg_image_recolor_opa(root, LV_OPA_TRANSP, 0);  // shown as is, not dimmed
}

void set_background(const void *image) {
  lv_style_set_bg_image_src(&screen_style, image);
  lv_obj_report_style_change(&screen_style);
}

}  // namespace theme
