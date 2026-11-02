#include "ui/widgets.h"

#include <stdio.h>
#include <string.h>

#include "assets/assets.h"
#include "board_config.h"
#include "fonts/fonts.h"
#include "ui/item_tile.h"
#include "ui/theme.h"

// Layouts: the gap between children, and ui_split's side padding (the panel's left and right edges
// are straight at mid-height, so it needs less than SAFE_PAD) and the gap in its right pane.
static constexpr int32_t GAP = 10, SPLIT_SIDE = 10, SPLIT_ROW_GAP = 8;
static constexpr int32_t QR_QUIET_ZONE = 6;  // scanners need a light margin around the code
static constexpr int32_t ICON_TILE_GAP = 8;  // ui_icon_tile: icon to label
static constexpr int32_t PLATE_RADIUS = 6, PLATE_PAD_HOR = 8, PLATE_PAD_VER = 3;
static constexpr uint32_t PLATE_BG = 0xf3f3f4;

// ui_value_button: the gap between the value and its arrow.
static constexpr int32_t VALUE_GAP = 6;
// ui_choice_veil: squares like the craft pickers', 3 a row.
static constexpr int32_t CHOICE = 72, CHOICE_GAP = 14, CHOICE_ROW_GAP = 12, CHOICES_PER_ROW = 3;
static constexpr lv_opa_t VEIL_OPA = LV_OPA_40;
static constexpr UiKeyColors CURRENT_CHOICE = {0x5b5e66, 0x3f4148, 0x1f2024, 0xffffff};

// ui_page_nav: an arrow either side of the count, close in and a little up: touch is unreliable
// near the panel's edges.
static constexpr int32_t NAV_W = 52, NAV_H = 34, NAV_FROM_CENTER = 78, NAV_BOTTOM = 12;
static_assert(NAV_BOTTOM + NAV_H == UI_PAGE_NAV_H, "UI_PAGE_NAV_H is where the arrows end");
// The count's height in Menlo 16, bare and on its plate, so it can be centered on the arrows.
static constexpr int32_t NAV_LABEL_H = 17, NAV_PLATE_H = 24;

static constexpr int32_t KEY_RADIUS = 12, KEY_PRESSED_DEPTH = 2;
static constexpr int32_t SQUISH_PERMILLE = 80;  // 8% wider and shorter at the deepest
static constexpr uint32_t SQUISH_MS = 80;       // squash; springing back takes twice as long

static constexpr lv_opa_t VHS_TEXT_OPA = LV_OPA_90, VHS_GHOST_OPA = LV_OPA_40;
static constexpr int32_t VHS_RADIUS = 6, VHS_BUTTON_PAD_VER = 6;  // list rows and buttons
static constexpr lv_opa_t VHS_PRESSED_OPA = LV_OPA_20;           // their white light while pressed
static constexpr lv_opa_t BARS_ON = LV_OPA_90, BARS_OFF = LV_OPA_30;
static constexpr int RSSI_3_BARS = -60, RSSI_2_BARS = -72;  // dBm, from here up

static lv_obj_t *flex_box(lv_obj_t *parent, lv_flex_flow_t flow) {
  lv_obj_t *box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_set_flex_flow(box, flow);
  lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
  return box;
}

UiSplit ui_split(lv_obj_t *root) {
  lv_obj_t *row = flex_box(root, LV_FLEX_FLOW_ROW);
  lv_obj_set_size(row, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_pad_hor(row, SPLIT_SIDE, 0);
  lv_obj_set_style_pad_ver(row, theme::SAFE_PAD, 0);
  lv_obj_set_style_pad_column(row, GAP, 0);

  UiSplit split;
  split.left = flex_box(row, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_size(split.left, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  split.right = flex_box(row, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_height(split.right, LV_SIZE_CONTENT);
  lv_obj_set_flex_grow(split.right, 1);
  lv_obj_set_style_pad_row(split.right, SPLIT_ROW_GAP, 0);
  return split;
}

lv_obj_t *ui_row(lv_obj_t *parent) {
  lv_obj_t *row = flex_box(parent, LV_FLEX_FLOW_ROW);
  lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_column(row, GAP, 0);
  return row;
}

lv_obj_t *ui_column(lv_obj_t *root) {
  lv_obj_t *col = flex_box(root, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_size(col, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_pad_all(col, theme::SAFE_PAD, 0);
  lv_obj_set_style_pad_row(col, GAP, 0);
  return col;
}

lv_obj_t *ui_title(lv_obj_t *parent, const char *text) {
  lv_obj_t *label = ui_value(parent, text);
  lv_obj_set_style_text_font(label, theme::font_title(), 0);
  return label;
}

lv_obj_t *ui_value(lv_obj_t *parent, const char *text) {
  lv_obj_t *label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(label, LV_PCT(100));
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  return label;
}

lv_obj_t *ui_fit_line(lv_obj_t *parent, const char *text, int32_t width,
                      std::initializer_list<const lv_font_t *> fonts) {
  const lv_font_t *font = *(fonts.end() - 1);  // smallest, if nothing fits
  for (const lv_font_t *f : fonts) {
    if (lv_text_get_width(text, strlen(text), f, 0) <= width) {
      font = f;
      break;
    }
  }
  lv_obj_t *label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(label, width);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  return label;
}

lv_obj_t *ui_text(lv_obj_t *parent, const char *text) {
  lv_obj_t *label = ui_value(parent, text);
  lv_obj_set_style_text_color(label, theme::muted(), 0);
  return label;
}

lv_obj_t *ui_qr(lv_obj_t *parent, const char *data, int32_t size) {
  lv_obj_t *qr = lv_qrcode_create(parent);
  lv_qrcode_set_size(qr, size);
  lv_qrcode_set_dark_color(qr, lv_color_black());
  lv_qrcode_set_light_color(qr, lv_color_white());
  lv_qrcode_update(qr, data, strlen(data));
  lv_obj_set_style_border_color(qr, lv_color_white(), 0);  // the quiet zone
  lv_obj_set_style_border_width(qr, QR_QUIET_ZONE, 0);
  return qr;
}

lv_obj_t *ui_button(lv_obj_t *parent, const char *label, lv_event_cb_t on_click, void *user_data) {
  lv_obj_t *btn = lv_button_create(parent);
  ui_squish(btn);
  lv_obj_set_flex_grow(btn, 1);
  lv_obj_add_event_cb(btn, on_click, LV_EVENT_CLICKED, user_data);
  lv_obj_t *text = lv_label_create(btn);
  lv_label_set_text(text, label);
  lv_obj_center(text);
  return btn;
}

lv_obj_t *ui_loading(lv_obj_t *parent) {
  // Frames sampled from assets/source/loading.gif by scripts/gif_frames.sh
  // (a real GIF decoder would need far more RAM than the board has).
  lv_obj_t *anim = lv_animimg_create(parent);
  lv_animimg_set_src(anim, img_loading_frames, IMG_LOADING_FRAME_COUNT);
  lv_animimg_set_duration(anim, IMG_LOADING_FRAME_COUNT * theme::LOADING_FRAME_MS);
  lv_animimg_set_repeat_count(anim, LV_ANIM_REPEAT_INFINITE);
  lv_animimg_start(anim);
  return anim;
}

lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *text) {
  lv_obj_t *label = lv_label_create(parent);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  if (text) lv_label_set_text(label, text);
  return label;
}

void ui_set_text_if_changed(lv_obj_t *label, const char *text) {
  if (strcmp(lv_label_get_text(label), text) != 0) lv_label_set_text(label, text);
}

void ui_set_enabled(lv_obj_t *obj, bool enabled) {
  if (lv_obj_has_state(obj, LV_STATE_DISABLED) == !enabled) return;  // already so
  lv_obj_set_state(obj, LV_STATE_DISABLED, !enabled);
  lv_obj_set_style_opa(obj, enabled ? LV_OPA_COVER : theme::DISABLED_OPA, 0);
}

UiIconTile ui_icon_tile(lv_obj_t *parent, const void *icon, const char *text, int32_t w, int32_t h,
                        Sound press) {
  lv_obj_t *btn = lv_button_create(parent);
  lv_obj_remove_style_all(btn);
  theme::style_tile(btn);
  ui_squish(btn, press);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(btn, ICON_TILE_GAP, 0);
  if (icon) lv_image_set_src(lv_image_create(btn), icon);
  lv_obj_t *label = lv_label_create(btn);
  lv_obj_set_style_text_font(label, &font_menlo_16, 0);
  lv_label_set_text(label, text);
  return {btn, label};
}

// The arrow is in Montserrat: Menlo has no symbols.
lv_obj_t *ui_value_button(lv_obj_t *parent, int32_t w, int32_t h, bool arrow, lv_obj_t **label) {
  lv_obj_t *btn = lv_button_create(parent);
  lv_obj_remove_style_all(btn);
  theme::style_tile(btn);
  ui_squish(btn);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(btn, VALUE_GAP, 0);
  *label = ui_label(btn, &font_menlo_16, lv_color_hex(theme::INK));
  if (arrow) ui_label(btn, &lv_font_montserrat_14, lv_color_hex(theme::INK), LV_SYMBOL_DOWN);
  return btn;
}

lv_obj_t *ui_choice_veil(lv_obj_t *parent, const std::vector<std::string> &choices, int current,
                         lv_event_cb_t on_choice, void *user_data) {
  lv_obj_t *veil = lv_obj_create(parent);
  lv_obj_remove_style_all(veil);
  lv_obj_set_size(veil, SCREEN_W, SCREEN_H);
  lv_obj_set_style_bg_color(veil, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(veil, VEIL_OPA, 0);
  lv_obj_remove_flag(veil, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *grid = lv_obj_create(veil);
  lv_obj_remove_style_all(grid);
  lv_obj_set_size(grid, CHOICES_PER_ROW * CHOICE + (CHOICES_PER_ROW - 1) * CHOICE_GAP, LV_SIZE_CONTENT);
  lv_obj_center(grid);
  lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(grid, CHOICE_GAP, 0);
  lv_obj_set_style_pad_row(grid, CHOICE_ROW_GAP, 0);
  lv_obj_remove_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
  for (size_t i = 0; i < choices.size(); i++) {
    UiItemTile choice = ui_item_tile(grid, CHOICE, Sound::ItemClick, /*flat=*/true);
    const UiKeyColors &colors = (int)i == current ? CURRENT_CHOICE : UI_WHITE_KEY;
    ui_item_tile_set(choice, nullptr, choices[i].c_str(), "", colors);
    lv_obj_set_style_text_font(choice.text, &font_menlo_16, 0);
    lv_obj_align(choice.text, LV_ALIGN_CENTER, 0, 0);  // no count under it
    lv_obj_set_user_data(choice.key, (void *)(intptr_t)i);
    lv_obj_add_event_cb(choice.key, on_choice, LV_EVENT_CLICKED, user_data);
  }
  return veil;
}

lv_obj_t *ui_bordered_button(lv_obj_t *parent, lv_event_cb_t on_click, void *user_data) {
  lv_obj_t *btn = on_click ? lv_button_create(parent) : lv_obj_create(parent);
  lv_obj_remove_style_all(btn);
  lv_obj_set_style_border_width(btn, 1, 0);
  lv_obj_set_style_border_color(btn, lv_color_hex(theme::BORDER), 0);
  if (!on_click) {
    lv_obj_remove_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    return btn;
  }
  lv_obj_set_style_bg_color(btn, lv_color_hex(theme::PRESSED_BG), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_STATE_PRESSED);
  ui_squish(btn);
  lv_obj_add_event_cb(btn, on_click, LV_EVENT_CLICKED, user_data);
  return btn;
}

lv_obj_t *ui_plate_label(lv_obj_t *parent) {
  lv_obj_t *label = ui_label(parent, &font_menlo_16, lv_color_hex(theme::INK));
  lv_obj_set_style_bg_color(label, lv_color_hex(PLATE_BG), 0);
  lv_obj_set_style_bg_opa(label, LV_OPA_80, 0);
  lv_obj_set_style_radius(label, PLATE_RADIUS, 0);
  lv_obj_set_style_pad_hor(label, PLATE_PAD_HOR, 0);
  lv_obj_set_style_pad_ver(label, PLATE_PAD_VER, 0);
  return label;
}

UiPageNav ui_page_nav(lv_obj_t *parent, bool plated, lv_event_cb_t on_page, void *user_data) {
  UiPageNav nav;
  for (int dir : {-1, 1}) {
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_remove_style_all(btn);
    theme::style_tile(btn);
    ui_squish(btn);
    lv_obj_set_size(btn, NAV_W, NAV_H);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, dir * NAV_FROM_CENTER, -NAV_BOTTOM);
    lv_obj_t *arrow = lv_label_create(btn);
    lv_label_set_text(arrow, dir < 0 ? LV_SYMBOL_LEFT : LV_SYMBOL_RIGHT);
    lv_obj_center(arrow);
    lv_obj_set_user_data(btn, (void *)(intptr_t)dir);
    lv_obj_add_event_cb(btn, on_page, LV_EVENT_CLICKED, user_data);
    (dir < 0 ? nav.prev : nav.next) = btn;
  }
  nav.label = plated ? ui_plate_label(parent) : ui_label(parent, &font_menlo_16, lv_color_hex(theme::INK));
  const int32_t label_h = plated ? NAV_PLATE_H : NAV_LABEL_H;
  lv_obj_align(nav.label, LV_ALIGN_BOTTOM_MID, 0, -NAV_BOTTOM - (NAV_H - label_h) / 2);
  return nav;
}

void ui_page_nav_set(const UiPageNav &nav, size_t page, size_t pages, bool can_prev, bool can_next) {
  lv_label_set_text_fmt(nav.label, "%u/%u", (unsigned)page, (unsigned)pages);
  ui_set_enabled(nav.prev, can_prev);
  ui_set_enabled(nav.next, can_next);
}

std::string ui_countdown(uint32_t ms) {
  uint32_t s = ms / 1000 + (ms % 1000 ? 1 : 0);
  char buf[16];
  if (s >= 3600) {
    snprintf(buf, sizeof(buf), "%uh %02umn %02us", (unsigned)(s / 3600), (unsigned)(s / 60 % 60),
             (unsigned)(s % 60));
  } else if (s >= 60) {
    snprintf(buf, sizeof(buf), "%umn %02us", (unsigned)(s / 60), (unsigned)(s % 60));
  } else {
    snprintf(buf, sizeof(buf), "%us", (unsigned)s);
  }
  return buf;
}

lv_obj_t *ui_keycap(lv_obj_t *parent, int32_t w, int32_t h, const UiKeyColors &colors, Sound press) {
  lv_obj_t *key = lv_button_create(parent);
  lv_obj_remove_style_all(key);
  lv_obj_set_size(key, w, h);
  lv_obj_set_style_radius(key, KEY_RADIUS, 0);
  lv_obj_set_style_bg_opa(key, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_grad_dir(key, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_border_width(key, 1, 0);
  lv_obj_set_style_border_opa(key, LV_OPA_50, 0);
  // The side: an unblurred shadow under the face. Pressing pushes the face down onto it.
  lv_obj_set_style_shadow_width(key, 2, 0);
  lv_obj_set_style_shadow_opa(key, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_offset_y(key, UI_KEY_DEPTH, 0);
  lv_obj_set_style_shadow_offset_y(key, KEY_PRESSED_DEPTH, LV_STATE_PRESSED);
  lv_obj_set_style_translate_y(key, UI_KEY_DEPTH - KEY_PRESSED_DEPTH, LV_STATE_PRESSED);
  theme::add_texture(key, theme::BUTTON_TEXTURE);
  ui_squish(key, press);
  ui_keycap_colors(key, colors);
  return key;
}

void ui_keycap_colors(lv_obj_t *key, const UiKeyColors &colors) {
  lv_obj_set_style_bg_color(key, lv_color_hex(colors.top), 0);
  lv_obj_set_style_bg_grad_color(key, lv_color_hex(colors.bottom), 0);
  lv_obj_set_style_border_color(key, lv_color_hex(colors.edge), 0);
  lv_obj_set_style_shadow_color(key, lv_color_hex(colors.edge), 0);
  lv_obj_set_style_text_color(key, lv_color_hex(colors.text), 0);
}

static void squish_step(void *var, int32_t t) {  // t: 0 (rest) to 100 (fully squished)
  auto *obj = static_cast<lv_obj_t *>(var);
  int32_t d = SQUISH_PERMILLE * t / 100;
  lv_obj_set_style_transform_scale_x(obj, LV_SCALE_NONE * (1000 + d) / 1000, 0);
  lv_obj_set_style_transform_scale_y(obj, LV_SCALE_NONE * (1000 - d) / 1000, 0);
}

void ui_squish(lv_obj_t *obj, Sound press) {
  lv_obj_set_style_transform_pivot_x(obj, LV_PCT(50), 0);
  lv_obj_set_style_transform_pivot_y(obj, LV_PCT(100), 0);
  lv_obj_add_event_cb(
      obj,
      [](lv_event_t *e) {
        play_sound((Sound)(intptr_t)lv_event_get_user_data(e));
        auto *target = static_cast<lv_obj_t *>(lv_event_get_current_target(e));
        lv_anim_delete(target, squish_step);  // pressed again mid-squish: start over
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, target);
        lv_anim_set_exec_cb(&a, squish_step);
        lv_anim_set_values(&a, 0, 100);
        lv_anim_set_duration(&a, SQUISH_MS);
        lv_anim_set_playback_duration(&a, SQUISH_MS * 2);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_start(&a);
      },
      LV_EVENT_PRESSED, (void *)(intptr_t)press);
}

lv_obj_t *ui_vhs_text(lv_obj_t *parent, const char *text, int32_t width) {
  lv_obj_t *vhs = lv_obj_create(parent);
  lv_obj_remove_style_all(vhs);
  lv_obj_set_size(vhs, width + 2, LV_SIZE_CONTENT);
  lv_obj_remove_flag(vhs, LV_OBJ_FLAG_CLICKABLE);
  // Children: red ghost (x 0), cyan ghost (x 2), then the white text on top (x 1).
  const struct {
    uint32_t color;
    int32_t x;
    lv_opa_t opa;
  } layers[] = {{0xff3b6b, 0, VHS_GHOST_OPA}, {0x3bf0ff, 2, VHS_GHOST_OPA}, {0xffffff, 1, VHS_TEXT_OPA}};
  for (const auto &layer : layers) {
    lv_obj_t *label = lv_label_create(vhs);
    lv_obj_set_width(label, width);
    lv_obj_set_x(label, layer.x);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label, &font_menlo_16, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(layer.color), 0);
    lv_obj_set_style_text_opa(label, layer.opa, 0);
    lv_label_set_text(label, text);
  }
  return vhs;
}

void ui_vhs_set_text(lv_obj_t *vhs, const char *text) {
  for (uint32_t i = 0; i < lv_obj_get_child_count(vhs); i++) {
    lv_label_set_text(lv_obj_get_child(vhs, i), text);
  }
}

void ui_vhs_one_line(lv_obj_t *vhs) {
  for (uint32_t i = 0; i < lv_obj_get_child_count(vhs); i++) {
    lv_obj_t *label = lv_obj_get_child(vhs, i);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
  }
}

void ui_vhs_set_width(lv_obj_t *vhs, int32_t width) {
  lv_obj_set_width(vhs, width + 2);
  for (uint32_t i = 0; i < lv_obj_get_child_count(vhs); i++) {
    lv_obj_set_width(lv_obj_get_child(vhs, i), width);
  }
}

lv_obj_t *ui_vhs_list(lv_obj_t *root, int32_t top) {
  lv_obj_t *list = lv_obj_create(root);
  lv_obj_remove_style_all(list);
  lv_obj_set_pos(list, UI_VHS_LIST_SIDE, top);
  lv_obj_set_size(list, SCREEN_W - 2 * UI_VHS_LIST_SIDE, SCREEN_H - top);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_bottom(list, theme::SAFE_PAD, 0);  // the last row clears the rounded corners
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
  return list;
}

lv_obj_t *ui_vhs_list_row(lv_obj_t *list, int32_t height, lv_event_cb_t on_click, void *user_data) {
  lv_obj_t *row = lv_obj_create(list);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, LV_PCT(100), height);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_hor(row, UI_VHS_ROW_PAD, 0);
  lv_obj_set_style_pad_column(row, UI_VHS_ROW_GAP, 0);
  lv_obj_set_style_radius(row, VHS_RADIUS, 0);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(row, LV_OBJ_FLAG_SCROLL_CHAIN_VER);  // dragging a row scrolls the list
  if (on_click) {
    lv_obj_add_event_cb(row, [](lv_event_t *) { play_sound(Sound::ItemClick); }, LV_EVENT_PRESSED, nullptr);
    lv_obj_set_style_bg_color(row, lv_color_white(), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(row, VHS_PRESSED_OPA, LV_STATE_PRESSED);
    lv_obj_add_event_cb(row, on_click, LV_EVENT_CLICKED, user_data);
  } else {
    lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);
  }
  return row;
}

lv_obj_t *ui_vhs_button(lv_obj_t *parent, const char *label, lv_event_cb_t on_click, void *user_data,
                        int32_t text_w) {
  lv_obj_t *btn = lv_button_create(parent);
  lv_obj_remove_style_all(btn);
  lv_obj_set_flex_grow(btn, 1);
  lv_obj_set_height(btn, LV_SIZE_CONTENT);
  lv_obj_set_style_radius(btn, VHS_RADIUS, 0);
  lv_obj_set_style_pad_ver(btn, VHS_BUTTON_PAD_VER, 0);
  lv_obj_set_style_bg_color(btn, lv_color_white(), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(btn, VHS_PRESSED_OPA, LV_STATE_PRESSED);
  ui_squish(btn);
  lv_obj_t *text = ui_vhs_text(btn, label, text_w);
  lv_obj_center(text);
  lv_obj_add_event_cb(btn, on_click, LV_EVENT_CLICKED, user_data);
  return btn;
}

lv_obj_t *ui_vhs_button_row(lv_obj_t *parent) {
  lv_obj_t *row = flex_box(parent, LV_FLEX_FLOW_ROW);
  lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  return row;
}

// Children: the dot, then the arcs from the inside out, all centered on the bottom middle.
lv_obj_t *ui_wifi_bars(lv_obj_t *parent, int32_t size) {
  const int32_t step = size / 6, line = step * 6 / 10 > 2 ? step * 6 / 10 : 2;  // ring spacing, arc width
  const int32_t height = size * 3 / 4;
  const lv_point_t center = {size / 2, height - step / 2};
  lv_obj_t *icon = lv_obj_create(parent);
  lv_obj_remove_style_all(icon);
  lv_obj_set_size(icon, size, height);
  lv_obj_remove_flag(icon, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *dot = lv_obj_create(icon);
  lv_obj_remove_style_all(dot);
  lv_obj_set_size(dot, step, step);
  lv_obj_set_pos(dot, center.x - step / 2, center.y - step / 2);
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(dot, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);

  for (int i = 1; i <= 3; i++) {
    const int32_t r = step / 2 + i * step + (i == 3 ? step / 2 : 0);  // the outer one a bit wider apart
    lv_obj_t *arc = lv_arc_create(icon);
    lv_obj_remove_style_all(arc);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(arc, 2 * r, 2 * r);
    lv_obj_set_pos(arc, center.x - r, center.y - r);
    lv_arc_set_bg_angles(arc, 225, 315);  // the top quarter (0 is 3 o'clock, clockwise)
    lv_obj_set_style_arc_width(arc, line, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
  }
  ui_wifi_bars_set(icon, 0);
  return icon;
}

void ui_wifi_bars_set(lv_obj_t *icon, int bars) {
  lv_obj_t *dot = lv_obj_get_child(icon, 0);
  lv_obj_set_style_bg_opa(dot, bars > 0 ? BARS_ON : BARS_OFF, 0);
  for (int i = 1; i <= 3; i++)
    lv_obj_set_style_arc_opa(lv_obj_get_child(icon, i), i <= bars ? BARS_ON : BARS_OFF, LV_PART_MAIN);
}

int ui_wifi_bars_for(int rssi) {
  if (rssi == 0) return 0;
  return rssi >= RSSI_3_BARS ? 3 : rssi >= RSSI_2_BARS ? 2 : 1;
}
