#include "ui/screens/menu_screen.h"

#include "fonts/fonts.h"
#include "hal/hal.h"
#include "ui/theme.h"
#include "ui/widgets.h"

static constexpr int32_t TOP_BAR_H = 44;  // Back button row
static constexpr int32_t GAP = 10;
static constexpr int32_t GRID_PAD_TOP = 2, GRID_PAD_BOTTOM = 14;  // top: room for the focus outline
static constexpr int32_t TILE_W = 125, TILE_H = 84, TILE_ROW_GAP = 8;  // row gap: icon to label
static constexpr int32_t ICON = 44;  // matches scripts/menu_icons.sh
static constexpr int32_t BATTERY_Y = 12;
static constexpr int32_t BELL_GAP = 14, BELL_TOUCH = 10;  // left of the battery; extra touch area around it
static constexpr int32_t LEVEL_GAP = 6;  // the battery's symbol to its percentage
static constexpr int BATTERY_LOW = 15;  // percent: red from here down
static constexpr uint32_t LOW_COLOR = 0xd23c3c;

static lv_obj_t *tile(lv_obj_t *parent, const MenuScreen::Item &item, int index) {
  lv_obj_t *btn = lv_button_create(parent);
  lv_obj_remove_style_all(btn);
  lv_obj_set_size(btn, TILE_W, TILE_H);
  theme::style_tile(btn);
  ui_squish(btn);
  lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(btn, TILE_ROW_GAP, 0);

  if (item.icon) {
    lv_obj_t *icon = lv_image_create(btn);
    lv_image_set_src(icon, item.icon);
  } else {
    // Placeholder: a pastel rounded square, a different hue per tile.
    const theme::UserPalette pal = theme::user_palette(index * 70 + 30);
    lv_obj_t *icon = lv_obj_create(btn);
    lv_obj_remove_style_all(icon);
    lv_obj_set_size(icon, ICON, ICON);
    lv_obj_set_style_radius(icon, 10, 0);
    lv_obj_set_style_bg_opa(icon, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(icon, pal.header, 0);
    lv_obj_set_style_bg_grad_color(icon, pal.surface, 0);
    lv_obj_set_style_bg_grad_dir(icon, LV_GRAD_DIR_VER, 0);
    lv_obj_remove_flag(icon, LV_OBJ_FLAG_CLICKABLE);
  }

  lv_obj_t *label = lv_label_create(btn);
  lv_label_set_text(label, item.label.c_str());
  lv_obj_set_style_text_font(label, &font_menlo_14, 0);
  return btn;
}

void MenuScreen::build(lv_obj_t *root) {
  theme::style_light_screen(root);

  // Tiles first, so the first tile (not Back) gets the initial button focus.
  lv_obj_t *grid = lv_obj_create(root);
  lv_obj_remove_style_all(grid);
  lv_obj_set_pos(grid, 0, TOP_BAR_H);
  lv_obj_set_size(grid, LV_PCT(100), LV_VER_RES - TOP_BAR_H);
  lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_hor(grid, GAP, 0);
  lv_obj_set_style_pad_top(grid, GRID_PAD_TOP, 0);
  lv_obj_set_style_pad_bottom(grid, GRID_PAD_BOTTOM, 0);
  lv_obj_set_style_pad_gap(grid, GAP, 0);
  lv_obj_set_scroll_dir(grid, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(grid, LV_SCROLLBAR_MODE_OFF);

  tiles_.clear();
  for (size_t i = 0; i < items_.size(); i++) {
    lv_obj_t *btn = tile(grid, items_[i], (int)i);
    lv_obj_add_event_cb(btn, onClick, LV_EVENT_CLICKED, &items_[i]);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    tiles_.push_back(btn);
  }

  // The battery, top right: its symbol, then the percentage; the bell left of it. The rows are
  // content-sized and flow from START, not END: LVGL under-measures content-sized END rows.
  lv_obj_t *battery = lv_obj_create(root);
  lv_obj_remove_style_all(battery);
  lv_obj_set_size(battery, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(battery, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(battery, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(battery, BELL_GAP, 0);
  lv_obj_remove_flag(battery, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_align(battery, LV_ALIGN_TOP_RIGHT, -theme::SAFE_PAD, BATTERY_Y);
  if (onBell_) {
    lv_obj_t *bell = lv_button_create(battery);
    lv_obj_remove_style_all(bell);
    lv_obj_set_ext_click_area(bell, BELL_TOUCH);
    ui_squish(bell);
    ui_label(bell, theme::font_title(), lv_color_hex(theme::INK), LV_SYMBOL_BELL);
    lv_obj_add_event_cb(bell, [](lv_event_t *e) {
      static_cast<MenuScreen *>(lv_event_get_user_data(e))->onBell_();
    }, LV_EVENT_CLICKED, this);
  }
  lv_obj_t *level = lv_obj_create(battery);  // the battery's symbol and percentage, closer together
  lv_obj_remove_style_all(level);
  lv_obj_set_size(level, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(level, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(level, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(level, LEVEL_GAP, 0);
  lv_obj_remove_flag(level, LV_OBJ_FLAG_CLICKABLE);
  batteryIcon_ = lv_label_create(level);
  batteryText_ = lv_label_create(level);
  lv_obj_set_style_text_font(batteryText_, &font_menlo_14, 0);
  batteryShown_ = -2;
  usbShown_ = -1;
  update();

  addBackButton(root);
}

// Tiles that need what isn't there yet (e.g. the game's data) greyed out; the battery, or the
// lightning on USB.
void MenuScreen::update() {
  for (size_t i = 0; i < items_.size(); i++) {
    if (!items_[i].enabled) continue;
    ui_set_enabled(tiles_[i], items_[i].enabled());
  }

  int percent = hal_battery_percent();
  int usb = hal_usb_connected();
  if (percent == batteryShown_ && usb == usbShown_) return;
  batteryShown_ = percent;
  usbShown_ = usb;
  const char *symbol = usb ? SHAPE_LIGHTNING
                       : percent >= 90 ? LV_SYMBOL_BATTERY_FULL
                       : percent >= 65 ? LV_SYMBOL_BATTERY_3
                       : percent >= 40 ? LV_SYMBOL_BATTERY_2
                       : percent >= BATTERY_LOW ? LV_SYMBOL_BATTERY_1
                                                : LV_SYMBOL_BATTERY_EMPTY;
  lv_label_set_text(batteryIcon_, symbol);
  lv_obj_set_style_text_font(batteryIcon_, usb ? &font_shapes_22 : theme::font_title(), 0);
  lv_obj_set_style_text_opa(batteryIcon_, usb ? theme::SHAPE_OPA : LV_OPA_COVER, 0);
  if (percent < 0) {
    lv_label_set_text(batteryText_, "--");
  } else {
    lv_label_set_text_fmt(batteryText_, "%d%%", percent);
  }
  lv_color_t color = lv_color_hex(percent >= 0 && percent < BATTERY_LOW ? LOW_COLOR : theme::INK);
  lv_obj_set_style_text_color(batteryIcon_, usb ? lv_color_black() : color, 0);
  lv_obj_set_style_text_color(batteryText_, color, 0);
}

void MenuScreen::onClick(lv_event_t *e) {
  auto *item = static_cast<Item *>(lv_event_get_user_data(e));
  if (item->run) item->run();
}
