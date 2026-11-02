#include "ui/screens/hunt_screen.h"

#include "app/app.h"
#include "assets/assets.h"
#include "board_config.h"
#include "fonts/fonts.h"
#include "hal/hal.h"
#include "ui/loot_animation.h"
#include "ui/theme.h"
#include "ui/widgets.h"

static constexpr lv_opa_t FOREST_DIM = LV_OPA_20;  // lighter than other screens: the forest is the view
// The keys' sides (UI_KEY_DEPTH) end at the safe area's bottom edge.
static constexpr int32_t KEY_H = 61, KEY_BOTTOM = SCREEN_H - theme::SAFE_PAD - UI_KEY_DEPTH;
static constexpr int32_t BEETLE_KEY_W = 138;
// A square key. The 128 px cheeseman.png drawn at 54 px fills it with a small margin (an exact half,
// 64 px, would be crisper but doesn't fit).
static constexpr int32_t CHEESEMAN_KEY = KEY_H, CHEESEMAN_ICON = 54;
static constexpr int32_t KEY_PAD = 8, LINE_GAP = 4;  // the beetle key's text: side inset, between lines
static constexpr int32_t DOT = 8, DOT_GAP = 6, DOT_BORDER = 2;  // the hunts left, under its label
static constexpr lv_point_t LOOT_CENTER = {SCREEN_W / 2, 80};  // above the keys

static constexpr UiKeyColors CLAIM_COLORS = {0xdcf28c, 0xbcd96a, 0x4f5d22, 0x66733a};
static constexpr UiKeyColors HUNT_COLORS = {0xf0cf72, 0xdcae4c, 0x6b4f1c, 0x8a6526};
static constexpr UiKeyColors WAITING_COLORS = {0xdfe0e3, 0xc4c6cb, 0x55585f, 0x6b6e78};
static constexpr UiKeyColors CHEESEMAN_COLORS = {0xf3e9d2, 0xe0d0ae, 0x6e6048, 0x6e6048};

void HuntScreen::build(lv_obj_t *root) {
  look_ = Look::None;
  resultSeq_ = app().beetleResultSeq();  // only results that arrive from now on
  lv_obj_set_style_bg_image_recolor_opa(root, FOREST_DIM, 0);

  beetle_ = ui_keycap(root, BEETLE_KEY_W, KEY_H, WAITING_COLORS, Sound::ClaimPress);
  lv_obj_set_pos(beetle_, theme::SAFE_PAD, KEY_BOTTOM - KEY_H);
  lv_obj_set_flex_flow(beetle_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(beetle_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(beetle_, LINE_GAP, 0);
  lv_obj_add_event_cb(beetle_, onBeetle, LV_EVENT_CLICKED, this);
  top_ = lv_label_create(beetle_);
  lv_obj_set_style_text_font(top_, &font_menlo_14, 0);
  lv_label_set_text(top_, "Ready in");
  main_ = lv_label_create(beetle_);
  lv_obj_set_width(main_, BEETLE_KEY_W - 2 * KEY_PAD);
  lv_label_set_long_mode(main_, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(main_, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(main_, "");
  dots_ = lv_obj_create(beetle_);
  lv_obj_remove_style_all(dots_);
  lv_obj_set_size(dots_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(dots_, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(dots_, DOT_GAP, 0);
  lv_obj_remove_flag(dots_, LV_OBJ_FLAG_CLICKABLE);
  for (int i = 0; i < BeetleState::MAX_HUNTS; i++) {
    lv_obj_t *dot = lv_obj_create(dots_);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, DOT, DOT);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dot, DOT_BORDER, 0);
    lv_obj_set_style_border_color(dot, lv_color_hex(HUNT_COLORS.text), 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(HUNT_COLORS.text), 0);
    lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE);
  }

  lv_obj_t *cheeseman = ui_keycap(root, CHEESEMAN_KEY, KEY_H, CHEESEMAN_COLORS);
  lv_obj_set_pos(cheeseman, SCREEN_W - theme::SAFE_PAD - CHEESEMAN_KEY, KEY_BOTTOM - KEY_H);
  lv_obj_add_event_cb(cheeseman, onCheeseman, LV_EVENT_CLICKED, this);
  lv_obj_t *icon = lv_image_create(cheeseman);
  lv_image_set_src(icon, &img_cheeseman);
  lv_obj_set_size(icon, CHEESEMAN_ICON, CHEESEMAN_ICON);
  lv_image_set_inner_align(icon, LV_IMAGE_ALIGN_STRETCH);
  lv_image_set_antialias(icon, false);
  lv_obj_center(icon);

  addBackButton(root, BackLook::Text);
  update();
}

HuntScreen::Look HuntScreen::currentLook() {
  const BeetleState &beetle = app().beetle();
  if (beetle.trapMsLeft(hal_millis()) == 0) return Look::Claim;
  if (beetle.huntsRemaining(hal_epoch_ms()) > 0) return Look::Hunt;
  return Look::Waiting;
}

void HuntScreen::update() {
  if (app().beetleResultSeq() != resultSeq_) {
    resultSeq_ = app().beetleResultSeq();
    showResult();
  }
  const BeetleState &beetle = app().beetle();
  Look look = currentLook();
  if (look != look_) {
    look_ = look;
    ui_keycap_colors(beetle_, look == Look::Claim  ? CLAIM_COLORS
                              : look == Look::Hunt ? HUNT_COLORS
                                                   : WAITING_COLORS);
    lv_obj_update_flag(dots_, LV_OBJ_FLAG_HIDDEN, look != Look::Hunt);
  }

  App::BeetleTask task = app().beetleTask(), failed = app().beetleFailed();
  // An action in flight (a background refresh isn't one), or the counters not read yet.
  bool busy = task != App::BeetleTask::None || !beetle.loaded;
  bool waiting = look == Look::Waiting && !busy;
  lv_obj_update_flag(top_, LV_OBJ_FLAG_HIDDEN, !waiting);

  std::string text = busy ? "..."
                     : waiting ? ui_countdown(beetle.beetlesMsLeft(hal_millis(), hal_epoch_ms()))
                     : failed == App::BeetleTask::Trap || failed == App::BeetleTask::Hunt ? "RETRY"
                     : look == Look::Claim ? "CLAIM BEETLE"
                                           : "HUNT BEETLE";
  ui_set_text_if_changed(main_, text.c_str());
  // The labels small, the countdown big.
  lv_obj_set_style_text_font(main_, waiting ? &font_menlo_16 : &font_menlo_14, 0);

  if (look == Look::Hunt) {
    int hunts = beetle.huntsRemaining(hal_epoch_ms());
    for (int i = 0; i < BeetleState::MAX_HUNTS; i++)
      lv_obj_set_style_bg_opa(lv_obj_get_child(dots_, i), i < hunts ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
  }
}

void HuntScreen::showResult() {
  const App::BeetleResult &result = app().beetleResult();
  if (result.task != App::BeetleTask::Trap && result.task != App::BeetleTask::Hunt) return;
  if (result.rateLimited) return;  // the popup covers it
  if (result.items.empty()) {  // a claim or hunt can come back empty-handed
    play_loot_message(root(), result.ok ? "Nothing.." : "Error", LOOT_CENTER);
    return;
  }
  std::vector<LootIcon> icons;
  for (const BeetleItem &item : result.items) {
    const lv_image_dsc_t *image = app().itemIcon(item.icon);
    icons.push_back({image ? image : &img_icon_beetle, item.secondary});  // icon fetch failed: generic beetle
  }
  play_loot(root(), icons, LOOT_CENTER);
  play_sound(Sound::HuntSuccess);
}

void HuntScreen::onBeetle(lv_event_t *e) {
  auto *self = static_cast<HuntScreen *>(lv_event_get_user_data(e));
  switch (self->currentLook()) {
    case Look::Claim: self->app().catchBeetle(); break;
    case Look::Hunt: self->app().huntBeetle(); break;
    default: play_sound(Sound::Error); break;  // cooling down (replaces the press sound)
  }
}

void HuntScreen::onCheeseman(lv_event_t *e) {
  static_cast<HuntScreen *>(lv_event_get_user_data(e))->app().openCheese();
}
