#pragma once

#include <lvgl.h>
#include <stdint.h>

// Shared look. Screens use these instead of hard-coding colors, fonts or images.
namespace theme {

inline lv_color_t bg() { return lv_color_hex(0x101418); }  // behind the background image
inline lv_color_t text() { return lv_color_white(); }
inline lv_color_t muted() { return lv_color_hex(0xb4bec8); }  // secondary text on the dark screens
inline lv_color_t accent() { return lv_color_hex(0x3fb950); }

inline const lv_font_t *font_body() { return &lv_font_montserrat_14; }
inline const lv_font_t *font_title() { return &lv_font_montserrat_20; }
inline const lv_font_t *font_small() { return &lv_font_montserrat_12; }
inline const lv_font_t *font_tiny() { return &lv_font_montserrat_10; }

// The light screens (Settings and what it opens, the menu, About...): dark text on pale grey.
// Hex values, so key colors (UiKeyColors) can use them too; lv_color_hex() for styles.
constexpr uint32_t INK = 0x2b2d33;         // text, outlines
constexpr uint32_t INK_MUTED = 0x6b6e78;   // captions, the version line
constexpr uint32_t INK_FAINT = 0x8e8e96;   // notes, a cooldown, a toast "button" without an action
constexpr uint32_t LIGHT_BG = 0xe6e8ec;    // under the dot texture
constexpr uint32_t BORDER = 0xb9b9bf;      // thin frames: the toast, its buttons, Poke back
constexpr uint32_t PRESSED_BG = 0xd6d6db;  // a bordered button while pressed

// Silhouettes drawn with the shapes font (the zzz, the craft switch's tools, the USB lightning):
// black, see-through.
constexpr lv_opa_t SHAPE_OPA = LV_OPA_50;
constexpr lv_opa_t DISABLED_OPA = LV_OPA_40;  // greyed-out buttons (ui_set_enabled)

// Darkens the background image so text on top stays readable (LV_OPA_0 = untouched art).
constexpr lv_opa_t BACKGROUND_DIM = LV_OPA_50;

constexpr lv_opa_t BUTTON_TEXTURE = LV_OPA_50;  // dots on buttons: half as strong as on cards

constexpr uint32_t LOADING_FRAME_MS = 60;  // 24 frames, 15 degrees each: the GIF's own speed

// The panel has rounded corners; keep content inside this inset.
constexpr int32_t SAFE_PAD = 18;

// Colors derived from the user's theme hue (whoami `color`, 0-360), with the website's formulas:
// the hue changes, while saturation and lightness are fixed per token.
struct UserPalette {
  lv_color_t surface;  // card body          hsl(h, 80%, 95%)
  lv_color_t header;   // card header strip  hsl(h, 60%, 85%)
  lv_color_t border;   // rules, frames      hsl(h, 50%, 75%)
  lv_color_t text;     // card text          hsl(h, 35%, 18%)
  lv_color_t accent;   // highlights         hsl(h, 60%, 45%)
};
UserPalette user_palette(int hue);
UserPalette default_palette();  // the website's "default" (white) theme: the same tokens, in grays

lv_color_t hsl(int h, int s, int l);  // h 0-360, s and l in percent

void init();  // once, before the first screen is built
void style_textured(lv_obj_t *obj, lv_color_t color);  // solid color + the faint dot texture
void add_texture(lv_obj_t *obj, lv_opa_t opa = LV_OPA_COVER);  // the dot texture over obj's own background
void style_tile(lv_obj_t *obj);  // whitish rounded tile with a soft shadow: menu tiles, Back button
void style_screen(lv_obj_t *root);
void style_light_screen(lv_obj_t *root);  // LIGHT_BG and the dot texture instead of the background image
void set_background(const void *image);  // updates every screen, open or not
// The RWO blue screen (VHS-style): device-level screens (startup, Wi-Fi). Pair with ui_vhs_text().
void style_blue_screen(lv_obj_t *root);

}  // namespace theme
