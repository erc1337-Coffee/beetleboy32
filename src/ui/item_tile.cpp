#include "ui/item_tile.h"

#include "fonts/fonts.h"
#include "ui/theme.h"

static constexpr int32_t ICON_PCT = 70;   // of the tile
static constexpr int32_t ICON_LIFT = 5;   // px up from the center, to make room for the count
static constexpr int32_t TEXT_MARGIN = 3;  // a name's room either side, inside the tile
// An empty tile's shape: a silhouette, fainter than theme::SHAPE_OPA's since it marks a free spot.
static constexpr lv_opa_t PLACEHOLDER_OPA = LV_OPA_40;
static constexpr int32_t BIG_SHAPE_FROM = 56;  // tiles this wide and up get the big shapes

static constexpr int32_t FLAT_RADIUS = 10, FLAT_EDGE = 3, FLAT_PRESSED_DROP = 2;

static lv_obj_t *flat_button(lv_obj_t *parent, int32_t size, Sound press) {
  lv_obj_t *b = lv_button_create(parent);
  lv_obj_remove_style_all(b);
  lv_obj_set_size(b, size, size);
  lv_obj_set_style_radius(b, FLAT_RADIUS, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_grad_dir(b, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_border_side(b, LV_BORDER_SIDE_BOTTOM, 0);  // a hint of the keycap's edge
  lv_obj_set_style_border_width(b, FLAT_EDGE, 0);
  lv_obj_set_style_translate_y(b, FLAT_PRESSED_DROP, LV_STATE_PRESSED);
  lv_obj_set_style_border_width(b, 1, LV_STATE_PRESSED);
  lv_obj_add_event_cb(b, [](lv_event_t *e) { play_sound((Sound)(intptr_t)lv_event_get_user_data(e)); },
                      LV_EVENT_PRESSED, (void *)(intptr_t)press);
  return b;
}

UiItemTile ui_item_tile(lv_obj_t *parent, int32_t size, Sound press, bool flat) {
  UiItemTile t;
  t.key = flat ? flat_button(parent, size, press) : ui_keycap(parent, size, size, ui_rarity_colors(0), press);
  t.icon = lv_image_create(t.key);
  const int32_t icon = ui_item_tile_icon_size(size);
  lv_obj_set_size(t.icon, icon, icon);
  lv_image_set_inner_align(t.icon, LV_IMAGE_ALIGN_CENTER);  // made to fit: no scaling
  lv_obj_align(t.icon, LV_ALIGN_CENTER, 0, -ICON_LIFT);
  t.count = lv_label_create(t.key);
  lv_obj_set_style_text_font(t.count, &font_menlo_14, 0);
  lv_obj_align(t.count, LV_ALIGN_BOTTOM_MID, 0, 1);
  t.text = lv_label_create(t.key);
  lv_label_set_long_mode(t.text, LV_LABEL_LONG_DOT);
  lv_obj_set_width(t.text, size - 2 * TEXT_MARGIN);
  lv_obj_set_style_text_align(t.text, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(t.text, &font_menlo_14, 0);
  lv_obj_align(t.text, LV_ALIGN_CENTER, 0, -ICON_LIFT);
  return t;
}

void ui_item_tile_set(const UiItemTile &t, const lv_image_dsc_t *icon, const char *text, const char *count,
                      const UiKeyColors &colors) {
  ui_keycap_colors(t.key, colors);
  lv_obj_update_flag(t.icon, LV_OBJ_FLAG_HIDDEN, !icon);
  if (icon) lv_image_set_src(t.icon, icon);
  lv_obj_update_flag(t.text, LV_OBJ_FLAG_HIDDEN, icon != nullptr);
  // LVGL symbols (Remove's X) are in Montserrat, shapes (4-byte UTF-8) in their own font, names in Menlo.
  const unsigned char lead = text ? (unsigned char)text[0] : 0;
  const bool shape = lead >= 0xF0, symbol = lead == 0xEF;
  const lv_font_t *shape_font = lv_obj_get_width(t.key) >= BIG_SHAPE_FROM ? &font_shapes_40 : &font_shapes_22;
  const lv_font_t *font = shape ? shape_font : symbol ? theme::font_title() : &font_menlo_14;
  lv_obj_set_style_text_font(t.text, font, 0);
  lv_obj_set_style_text_color(t.text, shape ? lv_color_black() : lv_color_hex(colors.text), 0);
  lv_obj_set_style_text_opa(t.text, shape ? PLACEHOLDER_OPA : LV_OPA_COVER, 0);
  lv_obj_align(t.text, LV_ALIGN_CENTER, 0, shape ? 0 : -ICON_LIFT);  // a shape has no count under it
  lv_label_set_text(t.text, text ? text : "");
  lv_label_set_text(t.count, count);
  lv_obj_update_flag(t.count, LV_OBJ_FLAG_HIDDEN, !count[0]);
}

int32_t ui_item_tile_icon_size(int32_t size) { return size * ICON_PCT / 100; }

// The website's tile colors, by rarity (approximately): brown, green, yellow, teal, pink, sky.
UiKeyColors ui_rarity_colors(int rarity) {
  switch (rarity) {
    case 1: return {0xa77656, 0x86593d, 0x3e2a1c, 0xf4ece4};
    case 2: return {0xc8deaa, 0xa2c083, 0x4a5d34, theme::INK};
    case 3: return {0xf4e690, 0xdac868, 0x6b5d1e, theme::INK};
    case 4: return {0x9fe2d2, 0x72c4b2, 0x2c5a50, theme::INK};
    case 5: return {0xeea0bf, 0xd67c9f, 0x6a2c44, theme::INK};
    case 6: return {0xbadff3, 0x8fc3e2, 0x2f5670, theme::INK};
    default: return {0xdfe0e3, 0xc4c6cb, 0x55585f, theme::INK};
  }
}
