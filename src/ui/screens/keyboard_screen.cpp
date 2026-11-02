#include "ui/screens/keyboard_screen.h"

#include <string.h>

#include "board_config.h"
#include "fonts/fonts.h"
#include "hal/hal.h"
#include "ui/theme.h"
#include "ui/widgets.h"
#include "util/secret.h"

// Layout (screen is 280x240): the field next to the Back button, then 2 rows of 3 group keys (about
// a fingertip each) and the row of 4 action keys.
static constexpr int32_t FIELD_X = 100, FIELD_Y = 6, FIELD_W = 162, FIELD_H = 30;
static constexpr int32_t KEYS_Y = 46, SIDE = 8, GAP = 8;
static constexpr int32_t KEY_H = 54, ROW_GAP = 10;  // the gap clears each key's 3D side
static constexpr int32_t CELL_W = (SCREEN_W - 2 * SIDE - 2 * GAP) / 3;
static constexpr int32_t ACTION_W = (SCREEN_W - 2 * SIDE - 3 * GAP) / 4;
static constexpr UiKeyColors KEY_COLORS = {0xf4f5f7, 0xd9dbe0, 0x3b3f4a, theme::INK};
static constexpr UiKeyColors ACTION_COLORS = {0xc9ccd4, 0xaeb2bc, 0x3b3f4a, theme::INK};
static constexpr UiKeyColors OK_COLORS = {0xdcf28c, 0xbcd96a, 0x4f5d22, 0x3d4a14};
static constexpr uint32_t CYCLE_MS = 900;  // a tap on the same key within this cycles; after it, it settles
static constexpr uint32_t CYCLING_BG = 0xffffff, CYCLING_TEXT = 0x1d3fd6;  // the character still cycling

// Each layer: six groups (an empty one hides its key). Wi-Fi passwords use any printable ASCII.
static const char *const LAYERS[][6] = {
    {"abcde", "fghij", "klmno", "pqrst", "uvwxy", "z.-_@"},
    {"ABCDE", "FGHIJ", "KLMNO", "PQRST", "UVWXY", "Z.-_@"},
    {"12345", "67890", "!?#$%", "&*+=/", "()<>:", ";,'\"~"},
    {"[]{}\\", "|^`", "", "", "", ""},
};
static const char *const NEXT_LAYER[] = {"ABC", "123", "#+=", "abc"};  // what the layer key switches to
static constexpr int LAYER_COUNT = sizeof(LAYERS) / sizeof(LAYERS[0]);

static lv_obj_t *key(lv_obj_t *root, int32_t x, int32_t y, int32_t w, const UiKeyColors &colors,
                     const char *text, const lv_font_t *font, lv_event_cb_t on_click, void *user_data,
                     lv_obj_t **label_out = nullptr) {
  lv_obj_t *k = ui_keycap(root, w, KEY_H, colors);
  lv_obj_set_pos(k, x, y);
  lv_obj_t *label = lv_label_create(k);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(colors.text), 0);
  lv_obj_center(label);
  lv_obj_add_event_cb(k, on_click, LV_EVENT_CLICKED, user_data);
  if (label_out) *label_out = label;
  return k;
}

KeyboardScreen::~KeyboardScreen() { wipe_secret(text_); }

void KeyboardScreen::build(lv_obj_t *root) {
  theme::style_blue_screen(root);

  lv_obj_t *box = lv_obj_create(root);
  lv_obj_remove_style_all(box);
  lv_obj_set_pos(box, FIELD_X, FIELD_Y);
  lv_obj_set_size(box, FIELD_W, FIELD_H);
  lv_obj_set_style_radius(box, 6, 0);
  lv_obj_set_style_border_width(box, 1, 0);
  lv_obj_set_style_border_color(box, lv_color_white(), 0);
  lv_obj_set_style_border_opa(box, LV_OPA_60, 0);
  lv_obj_set_style_pad_hor(box, 6, 0);
  lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
  field_ = lv_label_create(box);
  lv_obj_set_style_text_font(field_, &font_menlo_16, 0);
  lv_obj_set_style_text_color(field_, lv_color_white(), 0);
  lv_obj_set_style_bg_color(field_, lv_color_hex(CYCLING_BG), LV_PART_SELECTED);
  lv_obj_set_style_text_color(field_, lv_color_hex(CYCLING_TEXT), LV_PART_SELECTED);
  lv_obj_align(field_, LV_ALIGN_LEFT_MID, 0, 0);

  for (int i = 0; i < CELLS; i++) {
    int32_t x = SIDE + (i % 3) * (CELL_W + GAP), y = KEYS_Y + (i / 3) * (KEY_H + ROW_GAP);
    cells_[i] = key(root, x, y, CELL_W, KEY_COLORS, "", &font_menlo_16, onCell, this, &cellLabels_[i]);
    lv_obj_set_user_data(cells_[i], (void *)(intptr_t)i);
  }
  const int32_t y = KEYS_Y + 2 * (KEY_H + ROW_GAP);
  key(root, SIDE, y, ACTION_W, ACTION_COLORS, "", &font_menlo_16, onLayer, this, &layerLabel_);
  key(root, SIDE + (ACTION_W + GAP), y, ACTION_W, ACTION_COLORS, "space", &font_menlo_16, onSpace, this);
  key(root, SIDE + 2 * (ACTION_W + GAP), y, ACTION_W, ACTION_COLORS, LV_SYMBOL_BACKSPACE, theme::font_title(),
      onDelete, this);
  key(root, SIDE + 3 * (ACTION_W + GAP), y, ACTION_W, OK_COLORS, LV_SYMBOL_OK, theme::font_title(), onOk,
      this);

  addBackButton(root, BackLook::Text);
  showKeys();
  showField();
}

void KeyboardScreen::showKeys() {
  for (int i = 0; i < CELLS; i++) {
    lv_label_set_text(cellLabels_[i], LAYERS[layer_][i]);
    lv_obj_update_flag(cells_[i], LV_OBJ_FLAG_HIDDEN, LAYERS[layer_][i][0] == 0);
  }
  lv_label_set_text(layerLabel_, NEXT_LAYER[layer_]);
}

void KeyboardScreen::showField() {
  if (text_.empty()) {
    lv_label_set_text(field_, prompt_.c_str());
    lv_label_set_long_mode(field_, LV_LABEL_LONG_DOT);
    lv_obj_set_width(field_, FIELD_W - 12);
    lv_obj_set_style_text_opa(field_, LV_OPA_50, 0);
    return;
  }
  // Monospace: as many of the last characters as fit, then the cursor (none while one cycles).
  const int fit = (FIELD_W - 12) / lv_font_get_glyph_width(&font_menlo_16, 'M', 0) - 1;
  std::string shown = (int)text_.size() > fit ? text_.substr(text_.size() - fit) : text_;
  if (masked_) {
    for (size_t i = 0; i < shown.size(); i++)
      if (cycling_ < 0 || i + 1 < shown.size()) shown[i] = '*';
  }
  if (cycling_ < 0) shown += "_";
  lv_label_set_long_mode(field_, LV_LABEL_LONG_CLIP);
  lv_label_set_text(field_, shown.c_str());
  uint32_t end = (uint32_t)shown.size();
  lv_label_set_text_selection_start(field_, cycling_ < 0 ? LV_LABEL_TEXT_SELECTION_OFF : end - 1);
  lv_label_set_text_selection_end(field_, cycling_ < 0 ? LV_LABEL_TEXT_SELECTION_OFF : end);
  wipe_secret(shown);
  lv_obj_set_style_text_opa(field_, LV_OPA_COVER, 0);
}

void KeyboardScreen::settle() {
  if (cycling_ < 0) return;
  cycling_ = -1;
  showField();
}

void KeyboardScreen::update() {
  if (cycling_ >= 0 && hal_millis() - tappedAt_ >= CYCLE_MS) settle();
}

void KeyboardScreen::onCell(lv_event_t *e) {
  auto *self = static_cast<KeyboardScreen *>(lv_event_get_user_data(e));
  int i = (int)(intptr_t)lv_obj_get_user_data(static_cast<lv_obj_t *>(lv_event_get_current_target(e)));
  const char *group = LAYERS[self->layer_][i];
  const int count = (int)strlen(group);
  if (self->cycling_ == i && hal_millis() - self->tappedAt_ < CYCLE_MS) {
    self->cycleIndex_ = (self->cycleIndex_ + 1) % count;  // the same key again: the next character
    self->text_.back() = group[self->cycleIndex_];
  } else {
    self->cycling_ = i;  // another key (or later): the previous character stays, this one starts
    self->cycleIndex_ = 0;
    self->text_ += group[0];
  }
  self->tappedAt_ = hal_millis();
  self->showField();
}

void KeyboardScreen::onLayer(lv_event_t *e) {
  auto *self = static_cast<KeyboardScreen *>(lv_event_get_user_data(e));
  self->layer_ = (self->layer_ + 1) % LAYER_COUNT;
  self->settle();
  self->showKeys();
}

void KeyboardScreen::onSpace(lv_event_t *e) {
  auto *self = static_cast<KeyboardScreen *>(lv_event_get_user_data(e));
  self->cycling_ = -1;
  self->text_ += ' ';
  self->showField();
}

void KeyboardScreen::onDelete(lv_event_t *e) {
  auto *self = static_cast<KeyboardScreen *>(lv_event_get_user_data(e));
  self->cycling_ = -1;  // a cycling character goes too
  if (!self->text_.empty()) self->text_.pop_back();
  self->showField();
}

void KeyboardScreen::onOk(lv_event_t *e) {
  auto *self = static_cast<KeyboardScreen *>(lv_event_get_user_data(e));
  if (self->done_) self->done_(self->text_);
}
