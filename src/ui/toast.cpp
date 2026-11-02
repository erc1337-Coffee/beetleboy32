#include "ui/toast.h"

#include <string.h>

#include "fonts/fonts.h"
#include "ui/theme.h"
#include "ui/widgets.h"

// Look, in pixels.
static constexpr int32_t TOP = 12;          // from the screen's top edge
static constexpr int32_t TEXT_W = 200;      // wraps past this
static constexpr int32_t ICON_FREE_W = 24;  // icons up to this wide (symbols) don't narrow the text
static constexpr int32_t PICTURE_W = 250;   // with a picture: picture and text in this (clear of the corners)
static constexpr int32_t PAD = 6, GAP = 8;  // inside the badge, icon to text
static constexpr int32_t BORDER_W = 2, RADIUS = 6, SHADOW = 10;
// Buttons: equal bordered rectangles side by side, as wide as the text column.
static constexpr int32_t BUTTON_GAP = 4, BUTTON_PAD_VER = 3;
static constexpr uint32_t BG = 0xe9e9ec, TEXT = 0x5a5a62;
static constexpr uint32_t TEXT_ONLY = theme::INK_FAINT;  // a "button" without an action
static constexpr uint32_t FADE_MS = 250;

static lv_obj_t *current = nullptr;

// One toast's buttons, owned by it (freed with it).
using Buttons = std::vector<ToastButton>;

void hide_toast() {
  if (!current) return;
  lv_obj_delete_async(current);  // may be called from one of its own buttons
  current = nullptr;
}

static lv_obj_t *flex(lv_obj_t *parent, lv_flex_flow_t flow) {
  lv_obj_t *box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_set_size(box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(box, flow);
  lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(box, GAP, 0);
  lv_obj_set_style_pad_row(box, GAP / 2, 0);
  lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE);
  return box;
}

void show_toast(const void *icon, const char *text, uint32_t duration_ms, std::vector<ToastButton> buttons) {
  if (current) lv_obj_delete(current);  // its animations and buttons go with it

  lv_obj_t *toast = current = lv_obj_create(lv_layer_top());  // above every screen
  lv_obj_remove_style_all(toast);
  lv_obj_set_size(toast, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_align(toast, LV_ALIGN_TOP_MID, 0, TOP);
  lv_obj_set_style_bg_color(toast, lv_color_hex(BG), 0);
  lv_obj_set_style_bg_opa(toast, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(toast, lv_color_hex(theme::BORDER), 0);
  lv_obj_set_style_border_width(toast, BORDER_W, 0);
  lv_obj_set_style_radius(toast, RADIUS, 0);
  lv_obj_set_style_pad_all(toast, PAD, 0);
  lv_obj_set_style_pad_column(toast, GAP, 0);
  lv_obj_set_style_shadow_width(toast, SHADOW, 0);
  lv_obj_set_style_shadow_opa(toast, LV_OPA_30, 0);
  lv_obj_set_flex_flow(toast, LV_FLEX_FLOW_ROW);  // the icon, left of the text and its buttons
  lv_obj_set_flex_align(toast, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  // With buttons it's a small dialog: it keeps taps from the screen below. Without, they go through.
  lv_obj_update_flag(toast, LV_OBJ_FLAG_CLICKABLE, !buttons.empty());
  auto *owned = new Buttons(std::move(buttons));
  lv_obj_add_event_cb(toast, [](lv_event_t *e) {
    if (lv_event_get_current_target(e) == current) current = nullptr;
    delete static_cast<Buttons *>(lv_event_get_user_data(e));
  }, LV_EVENT_DELETE, owned);

  int32_t text_w = TEXT_W, picture_h = 0;
  if (icon) {
    lv_obj_t *image = lv_image_create(toast);
    lv_image_set_src(image, icon);
    lv_obj_update_layout(image);
    if (lv_obj_get_width(image) > ICON_FREE_W) {
      text_w = PICTURE_W - lv_obj_get_width(image) - GAP;
      picture_h = lv_obj_get_height(image);
    }
  }
  // The text above its buttons. With buttons, the column is text_w wide; next to a picture it is also
  // as tall as the picture, with the text centered in what the buttons leave and the buttons along
  // the bottom.
  lv_obj_t *column = flex(toast, LV_FLEX_FLOW_COLUMN);
  lv_obj_t *text_box = column;
  if (!owned->empty()) {
    lv_obj_set_width(column, text_w);
    if (picture_h) {
      lv_obj_set_height(column, picture_h);
      text_box = flex(column, LV_FLEX_FLOW_COLUMN);
      lv_obj_set_width(text_box, text_w);
      lv_obj_set_flex_grow(text_box, 1);
    }
  }
  lv_obj_t *label = ui_label(text_box, &font_menlo_16, lv_color_hex(TEXT), text);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  // Short text keeps its own width; longer wraps at text_w and stays centered.
  if (lv_text_get_width(text, (uint32_t)strlen(text), &font_menlo_16, 0) > text_w) {
    lv_obj_set_width(label, text_w);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  }

  if (!owned->empty()) {
    lv_obj_t *row = flex(column, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, BUTTON_GAP, 0);
    const int32_t n = (int32_t)owned->size(), button_w = (text_w - BUTTON_GAP * (n - 1)) / n;
    for (ToastButton &b : *owned) {
      // A rectangle either way; without an action, just its text, greyed.
      lv_event_cb_t on_click = nullptr;
      if (b.onPress) {
        on_click = [](lv_event_t *e) {
          std::function<void()> action = static_cast<ToastButton *>(lv_event_get_user_data(e))->onPress;
          hide_toast();  // frees the buttons later: `action` is a copy
          if (action) action();
        };
      }
      lv_obj_t *cell = ui_bordered_button(row, on_click, &b);
      lv_obj_set_size(cell, button_w, LV_SIZE_CONTENT);
      lv_obj_set_style_pad_ver(cell, BUTTON_PAD_VER, 0);
      lv_obj_center(ui_label(cell, &font_menlo_14, lv_color_hex(b.onPress ? TEXT : TEXT_ONLY), b.label));
    }
  }

  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, toast);
  if (!owned->empty() && duration_ms == 0) {  // a dialog: fade in, then stay
    lv_anim_set_exec_cb(&a, [](void *obj, int32_t opa) {
      lv_obj_set_style_opa(static_cast<lv_obj_t *>(obj), (lv_opa_t)opa, 0);
    });
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_duration(&a, FADE_MS);
  } else {
    // One animation over the whole life (LVGL replaces a second one on the same object and
    // callback): fade in, hold, fade out, gone.
    lv_anim_set_custom_exec_cb(&a, [](lv_anim_t *anim, int32_t t) {
      int32_t end = (int32_t)anim->duration, fade = (int32_t)FADE_MS;
      int32_t opa = t < fade         ? t * LV_OPA_COVER / fade
                    : t > end - fade ? (end - t) * LV_OPA_COVER / fade
                                     : LV_OPA_COVER;
      lv_obj_set_style_opa(static_cast<lv_obj_t *>(anim->var), (lv_opa_t)LV_CLAMP(0, opa, LV_OPA_COVER), 0);
    });
    lv_anim_set_values(&a, 0, (int32_t)duration_ms);
    lv_anim_set_duration(&a, duration_ms);
    lv_anim_set_completed_cb(&a, [](lv_anim_t *done) {
      lv_obj_delete_async(static_cast<lv_obj_t *>(done->var));
    });
  }
  lv_anim_start(&a);
}
