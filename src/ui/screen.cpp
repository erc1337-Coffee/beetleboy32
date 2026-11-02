#include "ui/screen.h"

#include "app/app.h"
#include "ui/theme.h"
#include "ui/widgets.h"

static constexpr int32_t BACK_X = 20, BACK_Y = 6;  // x clears the display's rounded corner
// Grown to the right and down from there (the corner stays put); its bottom stays above BELOW_BACK.
// Touch reaches BACK_TOUCH further on every side, unseen.
static constexpr int32_t BACK_PAD_LEFT = 6, BACK_PAD_RIGHT = 18, BACK_PAD_VER = 6;
static constexpr int32_t BACK_TOUCH = 8;
static constexpr int32_t TEXT_LOOK_PAD = 4;  // BackLook::Text: an unseen margin for a bigger touch target

Screen::~Screen() { destroyView(); }

void Screen::destroyView() {
  if (group_) lv_group_delete(group_);
  if (root_) lv_obj_delete(root_);
  group_ = nullptr;
  root_ = nullptr;
}

void Screen::create() {
  root_ = lv_obj_create(nullptr);
  theme::style_screen(root_);
  // Screens are laid out to fit. What overflows on purpose (the cheeseman's feet, loot falling off
  // the bottom) must not make the whole screen draggable.
  lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
  group_ = lv_group_create();

  // Focusable widgets created during build() join this screen's group automatically.
  lv_group_t *prev_default = lv_group_get_default();
  lv_group_set_default(group_);
  build(root_);
  lv_group_set_default(prev_default);
}

lv_obj_t *Screen::addBackButton(lv_obj_t *parent, BackLook look) {
  lv_obj_t *btn = lv_button_create(parent);
  lv_obj_remove_style_all(btn);
  if (look == BackLook::Tile) {
    theme::style_tile(btn);
    theme::add_texture(btn, theme::BUTTON_TEXTURE);
    lv_obj_set_style_pad_left(btn, BACK_PAD_LEFT, 0);  // the arrow sits close to the edge
    lv_obj_set_style_pad_right(btn, BACK_PAD_RIGHT, 0);
    lv_obj_set_style_pad_ver(btn, BACK_PAD_VER, 0);
  } else {
    lv_obj_set_style_text_color(btn, lv_color_white(), 0);
    lv_obj_set_style_opa(btn, LV_OPA_60, LV_STATE_PRESSED);
    lv_obj_set_style_pad_all(btn, TEXT_LOOK_PAD, 0);
    lv_obj_set_style_pad_right(btn, BACK_PAD_RIGHT, 0);
    lv_obj_set_style_pad_bottom(btn, BACK_PAD_VER + TEXT_LOOK_PAD, 0);
  }
  lv_obj_set_pos(btn, BACK_X, BACK_Y);
  lv_obj_set_ext_click_area(btn, BACK_TOUCH);
  ui_squish(btn);
  lv_obj_t *label = lv_label_create(btn);
  lv_label_set_text(label, LV_SYMBOL_LEFT " Back");
  lv_obj_add_event_cb(
      btn,
      [](lv_event_t *e) {
        auto *self = static_cast<Screen *>(lv_event_get_user_data(e));
        self->app().handleInput({InputSource::Touch, InputAction::Back});
      },
      LV_EVENT_CLICKED, this);
  return btn;
}
