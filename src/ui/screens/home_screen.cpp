#include "ui/screens/home_screen.h"

#include <ctype.h>
#include <limits.h>

#include "app/app.h"
#include "assets/assets.h"
#include "fonts/fonts.h"
#include "hal/hal.h"
#include "media/sounds.h"
#include "ui/theme.h"
#include "ui/widgets.h"

// Card geometry (screen is 280x240). Below the header: the name row, then avatar + stats.
// Inset so the sleeve clears the display's rounded corners; its bottom stays above the bio plate.
static constexpr int32_t CARD_X = 20, CARD_Y = 18, CARD_W = 240, CARD_H = 176;
static constexpr int32_t CARD_RADIUS = 14, CARD_SHADOW = 6;
static constexpr int32_t HEADER_H = 30;
static constexpr int32_t PAD = 10;                    // content inset inside the card
static constexpr int32_t NAME_Y = 38, RULE_Y = 64;    // row 1
static constexpr int32_t ROW2_Y = 74, AVATAR = 96;    // row 2
static constexpr int32_t STATS_X = PAD + AVATAR + 14;
static constexpr int32_t LEVEL_Y = ROW2_Y + 6, CHEESE_Y = ROW2_Y + 38, BEETLES_Y = ROW2_Y + 68;
static constexpr int32_t LEVEL_PAD_HOR = 6, LEVEL_PAD_VER = 1, LEVEL_RADIUS = 2;
static constexpr int32_t STAT_ICON = 20;              // the stat icons' size
static constexpr int32_t STAT_TEXT_X = 26, STAT_TEXT_DY = 3;  // the value, from the icon's corner
static constexpr int32_t READY_DOT = 8;  // red dot at the end of a stat row: something to claim
static constexpr int32_t READY_DOT_X = STATS_X + 90;  // same x on every row; just past the LEVEL box
static constexpr uint32_t READY_DOT_COLOR = 0xe5484d;
static constexpr int32_t RULE_DIAMOND = 5;            // the rule's end diamonds, before rotating
// The seal over the card's bottom-right corner (the card clips what spills over).
static constexpr int32_t SEAL_INSET = 40, SEAL_ROTATION = 150;  // rotation: 0.1 degree
static constexpr lv_opa_t SEAL_OPA = LV_OPA_COVER / 4;

// The silver frame around everything, a vertical gradient.
static constexpr uint32_t FRAME_TOP = 0xdcdde2, FRAME_BOTTOM = 0x9c9ea6;
// The plastic sleeve around the card.
static constexpr int32_t SLEEVE_MARGIN = 6, SLEEVE_RADIUS = 18, SLEEVE_SHADOW = 10;
static constexpr uint32_t SLEEVE_GRADIENT = 0xb9bac0, EDGE_SHADOW = 0x50525a;  // shadow: sleeve and card
// The bio plate under the card.
static constexpr int32_t PLATE_X = 24, PLATE_Y = 202, PLATE_W = 232, PLATE_H = 30, PLATE_RADIUS = 6;
static constexpr int32_t BIO_W = 216;
static constexpr uint32_t PLATE_TOP = 0xf2f2f4, PLATE_BOTTOM = 0xcfd0d5, PLATE_BORDER = 0xb4b5bb;
static constexpr uint32_t BIO_COLOR = 0x7d7e85;

static std::string thousands(long n) {
  std::string s = std::to_string(n);
  for (int i = (int)s.size() - 3; i > (n < 0 ? 1 : 0); i -= 3) s.insert(i, ",");
  return s;
}

static lv_obj_t *box(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, lv_color_t bg) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_color(o, bg, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  return o;
}

// Horizontal rule with diamond ends, like the website's card.
static void rule(lv_obj_t *card, int32_t x, int32_t y, int32_t w, lv_color_t color) {
  box(card, x, y, w, 1, color);
  const int32_t half = RULE_DIAMOND / 2;
  for (int32_t dx : {x - half, x + w - 1 - half}) {
    lv_obj_t *d = box(card, dx, y - half, RULE_DIAMOND, RULE_DIAMOND, color);
    lv_obj_set_style_transform_rotation(d, 450, 0);
    lv_obj_set_style_transform_pivot_x(d, half, 0);
    lv_obj_set_style_transform_pivot_y(d, half, 0);
  }
}

// Icon + value (filled in by update()), and the row's ready dot (hidden), right-aligned so dots
// line up across rows.
static HomeScreen::Stat stat(lv_obj_t *card, int32_t y, const void *icon, lv_color_t color) {
  lv_obj_t *img = lv_image_create(card);
  lv_image_set_src(img, icon);
  lv_obj_set_pos(img, STATS_X, y);
  lv_obj_t *value = ui_label(card, &font_menlo_14, color, "");
  lv_obj_set_pos(value, STATS_X + STAT_TEXT_X, y + STAT_TEXT_DY);
  lv_obj_t *dot = box(card, READY_DOT_X, y + STAT_ICON / 2 - READY_DOT / 2, READY_DOT, READY_DOT,
                      lv_color_hex(READY_DOT_COLOR));  // centered on the icon's row
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
  return {value, dot, -1};
}

static constexpr long LOADING = LONG_MIN;  // Stat::shown while the first read is on its way
static constexpr int LEVEL_LOADING = -2;   // levelShown_ likewise

static void show_loading(HomeScreen::Stat &stat) {
  if (stat.shown == LOADING) return;
  stat.shown = LOADING;
  lv_label_set_text(stat.value, "...");
}

static void show_value(HomeScreen::Stat &stat, long value) {
  if (value == stat.shown) return;
  stat.shown = value;
  lv_label_set_text(stat.value, thousands(value).c_str());
}

// Translucent plastic sleeve around the card: glossy gradient, bright rim, soft blurred edge.
static void make_sleeve(lv_obj_t *root) {
  lv_obj_t *sleeve = box(root, CARD_X - SLEEVE_MARGIN, CARD_Y - SLEEVE_MARGIN, CARD_W + 2 * SLEEVE_MARGIN,
                         CARD_H + 2 * SLEEVE_MARGIN, lv_color_white());
  lv_obj_set_style_bg_opa(sleeve, LV_OPA_60, 0);
  lv_obj_set_style_bg_grad_color(sleeve, lv_color_hex(SLEEVE_GRADIENT), 0);
  lv_obj_set_style_bg_grad_dir(sleeve, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_radius(sleeve, SLEEVE_RADIUS, 0);
  lv_obj_set_style_border_width(sleeve, 1, 0);
  lv_obj_set_style_border_color(sleeve, lv_color_white(), 0);
  lv_obj_set_style_border_opa(sleeve, LV_OPA_80, 0);
  lv_obj_set_style_shadow_width(sleeve, SLEEVE_SHADOW, 0);
  lv_obj_set_style_shadow_color(sleeve, lv_color_hex(EDGE_SHADOW), 0);
  lv_obj_set_style_shadow_opa(sleeve, LV_OPA_40, 0);
}

// The card itself, in the user's colors, with its header strip.
static lv_obj_t *make_card(lv_obj_t *root, const theme::UserPalette &pal) {
  lv_obj_t *card = box(root, CARD_X, CARD_Y, CARD_W, CARD_H, pal.surface);
  theme::style_textured(card, pal.surface);
  lv_obj_set_style_radius(card, CARD_RADIUS, 0);
  lv_obj_set_style_clip_corner(card, true, 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, pal.border, 0);
  lv_obj_set_style_shadow_width(card, CARD_SHADOW, 0);  // the card's edge, blurred through the plastic
  lv_obj_set_style_shadow_color(card, lv_color_hex(EDGE_SHADOW), 0);
  lv_obj_set_style_shadow_opa(card, LV_OPA_50, 0);

  lv_obj_t *header = box(card, 0, 0, CARD_W, HEADER_H, pal.header);
  lv_obj_center(ui_label(header, &font_hei_18, pal.text, "REMILIA 集团存在证明"));
  return card;
}

// Bottom plate with the bio, scrolling when it doesn't fit.
static void make_bio_plate(lv_obj_t *root, const std::string &bio) {
  lv_obj_t *plate = box(root, PLATE_X, PLATE_Y, PLATE_W, PLATE_H, lv_color_hex(PLATE_TOP));
  lv_obj_set_style_bg_grad_color(plate, lv_color_hex(PLATE_BOTTOM), 0);
  lv_obj_set_style_bg_grad_dir(plate, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_radius(plate, PLATE_RADIUS, 0);
  lv_obj_set_style_border_width(plate, 1, 0);
  lv_obj_set_style_border_color(plate, lv_color_hex(PLATE_BORDER), 0);
  std::string upper = bio;
  for (char &c : upper) c = (char)toupper((unsigned char)c);
  lv_obj_t *label = ui_label(plate, &font_menlo_14, lv_color_hex(BIO_COLOR), upper.c_str());
  lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_width(label, BIO_W);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(label);
}

void HomeScreen::build(lv_obj_t *root) {
  const theme::UserPalette pal = app().palette();

  // A plastic card in a silver frame instead of the time-of-day background.
  lv_obj_set_style_bg_image_src(root, nullptr, 0);
  lv_obj_set_style_bg_color(root, lv_color_hex(FRAME_TOP), 0);
  lv_obj_set_style_bg_grad_color(root, lv_color_hex(FRAME_BOTTOM), 0);
  lv_obj_set_style_bg_grad_dir(root, LV_GRAD_DIR_VER, 0);
  make_sleeve(root);
  lv_obj_t *card = make_card(root, pal);

  // Row 1: display name, centered over a full-width rule.
  lv_obj_t *name = ui_label(card, &font_menlo_16, pal.text, profile_.displayName.c_str());
  lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
  lv_obj_set_width(name, CARD_W - 2 * PAD);
  lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(name, PAD, NAME_Y);
  rule(card, PAD, RULE_Y, CARD_W - 2 * PAD, pal.border);

  // Row 2: avatar on the left, level and stats on the right.
  lv_obj_t *avatar = avatarBox_ = box(card, PAD, ROW2_Y, AVATAR, AVATAR, pal.header);  // the image goes in
  lv_obj_set_style_border_width(avatar, 1, 0);
  lv_obj_set_style_border_color(avatar, pal.border, 0);
  avatarShown_ = false;  // update() adds it, now or once downloaded

  // Level, boxed like the card's "NOT FOR SALE" stamp.
  lv_obj_t *level = level_ = ui_label(card, &font_menlo_14, pal.text, "");
  levelShown_ = -1;
  lv_obj_set_style_border_width(level, 1, 0);
  lv_obj_set_style_border_color(level, pal.text, 0);
  lv_obj_set_style_pad_hor(level, LEVEL_PAD_HOR, 0);
  lv_obj_set_style_pad_ver(level, LEVEL_PAD_VER, 0);
  lv_obj_set_style_radius(level, LEVEL_RADIUS, 0);
  lv_obj_set_pos(level, STATS_X, LEVEL_Y);
  cheese_ = stat(card, CHEESE_Y, &img_icon_cheese, pal.text);
  beetles_ = stat(card, BEETLES_Y, &img_icon_beetle, pal.text);
  update();

  // The seal, stamped over the card's corner.
  lv_obj_t *seal = lv_image_create(card);
  lv_image_set_src(seal, &img_seal);
  lv_image_set_rotation(seal, SEAL_ROTATION);
  lv_obj_set_style_image_opa(seal, SEAL_OPA, 0);
  lv_obj_set_pos(seal, CARD_W - SEAL_INSET, CARD_H - SEAL_INSET);

  make_bio_plate(root, profile_.bio);
}

void HomeScreen::onShow() { app().refreshBeetle(); }  // cheap: skipped if read moments ago

// Live: a refresh can land while Home is on screen.
void HomeScreen::update() {
  if (!avatarShown_ && !profile_.avatar.empty()) {
    avatarShown_ = true;
    lv_image_set_src(lv_image_create(avatarBox_), profile_.avatar.dsc());
  }

  const BeetleState &beetle = app().beetle();
  const uint32_t now = hal_millis();
  if (!beetle.loaded) {
    show_loading(cheese_);
    show_loading(beetles_);
    if (levelShown_ != LEVEL_LOADING) {
      levelShown_ = LEVEL_LOADING;
      lv_label_set_text(level_, "LEVEL ...");
    }
    lv_obj_add_flag(cheese_.dot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(beetles_.dot, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  show_value(cheese_, beetle.cheese);
  show_value(beetles_, beetle.beetles);
  if (beetle.level != levelShown_) {
    levelShown_ = beetle.level;
    lv_label_set_text_fmt(level_, "LEVEL %d", beetle.level);
  }
  lv_obj_update_flag(cheese_.dot, LV_OBJ_FLAG_HIDDEN, beetle.cheeseMsLeft(now) > 0);
  lv_obj_update_flag(beetles_.dot, LV_OBJ_FLAG_HIDDEN, !beetle.beetlesReady(now, hal_epoch_ms()));
}

bool HomeScreen::onInput(const InputEvent &event) {
  if (event.action != InputAction::Tap && event.action != InputAction::Confirm) return false;
  play_sound(Sound::Click);
  app().openMenu();
  return true;
}
