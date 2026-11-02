#pragma once

#include <lvgl.h>

#include "ui/widgets.h"

// An item as a keycap, like the website's craft bench: its icon over its count, colored by rarity.
// `flat`: a plain rounded button instead (no texture, shadow or squish): cheap enough for a grid of
// a hundred.
struct UiItemTile {
  lv_obj_t *key = nullptr;    // the keycap (the button)
  lv_obj_t *icon = nullptr;
  lv_obj_t *count = nullptr;  // under the icon
  lv_obj_t *text = nullptr;   // in the middle: a symbol, or the name while the icon isn't there
};
UiItemTile ui_item_tile(lv_obj_t *parent, int32_t size, Sound press = Sound::ItemClick, bool flat = false);
// `icon` nullptr: `text` instead: a name, a symbol, or a SHAPE_ (fonts.h) drawn as a faint
// silhouette, like a shadow of what goes there. `count` "" hides it.
void ui_item_tile_set(const UiItemTile &tile, const lv_image_dsc_t *icon, const char *text, const char *count,
                      const UiKeyColors &colors);
UiKeyColors ui_rarity_colors(int rarity);  // 0: unknown (grey)
// The icon's box in a tile of `size`: icons are made to fit it (App::itemIcon(path, size)) and drawn
// 1:1, centered.
int32_t ui_item_tile_icon_size(int32_t size);
