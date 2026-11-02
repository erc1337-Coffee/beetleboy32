#include "ui/screens/cheese_screen.h"

#include "app/app.h"
#include "assets/assets.h"
#include "board_config.h"
#include "fonts/fonts.h"
#include "hal/hal.h"
#include "media/sounds.h"
#include "ui/loot_animation.h"
#include "ui/theme.h"

// Scene layout, in pixels.
static constexpr struct {
  int32_t cheeseman_size = 192;  // drawn size; cheeseman.png is 128x128
  int32_t cheeseman_x = 0;       // offset from the center, + = right
  int32_t cheeseman_sink = 48;   // how far he goes below the bottom edge
  int32_t squish = 12;           // percent wider/shorter at the bottom of a squish
  int32_t squish_ms = 90;        // squash; springing back takes twice as long
  int32_t claim_y = 56;          // center of the claim button / countdown, from the top
  int32_t claim_w = 132;
  int32_t claim_h = 40;
} SCENE{};

static constexpr int32_t CHEESEMAN_SRC = 128;
static constexpr uint32_t COOLDOWN_COLOR = 0xf2b711;
static constexpr int SQUISHES_FOR_JUNK = 3;
static constexpr lv_point_t LOOT_CENTER = {SCREEN_W / 2, 110};  // junk shows up here
static constexpr uint32_t SCALE = SCENE.cheeseman_size * LV_SCALE_NONE / CHEESEMAN_SRC;

// t: 0 (rest) to 100 (fully squished). Wider and shorter, feet on the ground (bottom pivot).
static void squish_cb(void *img, int32_t t) {
  auto *obj = static_cast<lv_obj_t *>(img);
  lv_image_set_scale_x(obj, SCALE * (100 * 100 + SCENE.squish * t) / (100 * 100));
  lv_image_set_scale_y(obj, SCALE * (100 * 100 - SCENE.squish * t) / (100 * 100));
}

void CheeseScreen::build(lv_obj_t *root) {
  ready_ = -1;  // rebuilt (e.g. after a theme change): set the poster again
  lv_obj_set_style_bg_image_recolor_opa(root, LV_OPA_TRANSP, 0);  // the poster is the scene: no dimming

  // Scaling keeps the object 128x128 and draws around the pivot: at his feet, so he grows (and
  // squishes) upward from where he stands.
  cheeseman_ = lv_image_create(root);
  lv_image_set_src(cheeseman_, &img_cheeseman);
  lv_image_set_pivot(cheeseman_, CHEESEMAN_SRC / 2, CHEESEMAN_SRC);
  lv_image_set_scale(cheeseman_, SCALE);
  lv_image_set_antialias(cheeseman_, false);  // pixel art: keep it sharp
  lv_obj_align(cheeseman_, LV_ALIGN_BOTTOM_MID, SCENE.cheeseman_x, SCENE.cheeseman_sink);
  lv_obj_add_flag(cheeseman_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(cheeseman_, SCENE.cheeseman_size - CHEESEMAN_SRC);  // the drawn size, roughly
  lv_obj_add_event_cb(cheeseman_, onSquish, LV_EVENT_PRESSED, this);
  squishes_ = 0;
  resultSeq_ = app().beetleResultSeq();  // only results that arrive from now on

  claim_ = ui_icon_tile(root, nullptr, "", SCENE.claim_w, SCENE.claim_h, Sound::ClaimPress);
  theme::add_texture(claim_.button, theme::BUTTON_TEXTURE);
  lv_obj_align(claim_.button, LV_ALIGN_TOP_MID, 0, SCENE.claim_y - SCENE.claim_h / 2);
  lv_obj_add_event_cb(claim_.button, onClaim, LV_EVENT_CLICKED, this);

  cooldown_ = lv_obj_create(root);
  lv_obj_remove_style_all(cooldown_);
  lv_obj_set_size(cooldown_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(cooldown_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(cooldown_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_text_color(cooldown_, lv_color_hex(COOLDOWN_COLOR), 0);
  lv_obj_t *ready_in = lv_label_create(cooldown_);
  lv_obj_set_style_text_font(ready_in, &font_menlo_14, 0);
  lv_label_set_text(ready_in, "Ready in");
  countdown_ = lv_label_create(cooldown_);
  lv_obj_set_style_text_font(countdown_, &font_menlo_16, 0);
  lv_label_set_text(countdown_, "");
  lv_obj_align(cooldown_, LV_ALIGN_CENTER, 0, SCENE.claim_y - SCREEN_H / 2);

  addBackButton(root, BackLook::Text);
  update();
}

void CheeseScreen::showResult() {
  const App::BeetleResult &result = app().beetleResult();
  if (result.task != App::BeetleTask::Cheese && result.task != App::BeetleTask::Junk) return;
  if (result.rateLimited) return;  // the popup covers it
  if (!result.ok) {
    play_loot_message(root(), "Error", LOOT_CENTER);
  } else if (result.task == App::BeetleTask::Cheese) {
    animatedUntil_ = hal_millis() + play_rain(root(), &img_icon_cheese);
    play_sound(Sound::Triumph);
  } else if (result.items.empty()) {
    play_loot_message(root(), "Nothing..", LOOT_CENTER);
  } else {
    std::vector<LootIcon> icons;
    for (const BeetleItem &item : result.items) {
      const lv_image_dsc_t *image = app().itemIcon(item.icon);
      icons.push_back({image ? image : &img_icon_beetle, item.secondary});  // icon fetch failed: generic
    }
    animatedUntil_ = hal_millis() + play_loot(root(), icons, LOOT_CENTER);
    play_sound(Sound::Achievement);
  }
}

void CheeseScreen::update() {
  if (app().beetleResultSeq() != resultSeq_) {
    resultSeq_ = app().beetleResultSeq();
    showResult();
  }
  uint32_t left = app().beetle().cheeseMsLeft(hal_millis());
  bool busy = app().beetleTask() != App::BeetleTask::None || !app().beetle().loaded;

  int ready = left == 0;
  if (ready != ready_) {
    ready_ = ready;
    lv_obj_set_style_bg_image_src(root(), ready ? &img_cheese_claim_poster : &img_cheese_empty_poster, 0);
  }

  // Cooling down: just the text. Otherwise the button (also while a claim is running).
  bool waiting = left && !busy;
  bool animating = (int32_t)(hal_millis() - animatedUntil_) < 0;
  lv_obj_update_flag(cooldown_, LV_OBJ_FLAG_HIDDEN, !waiting || animating);
  lv_obj_update_flag(claim_.button, LV_OBJ_FLAG_HIDDEN, waiting);
  const char *ready_text = app().beetleFailed() == App::BeetleTask::Cheese ? "RETRY" : "CLAIM";
  std::string text = waiting ? ui_countdown(left) : busy ? "..." : ready_text;
  lv_obj_t *label = waiting ? countdown_ : claim_.label;
  ui_set_text_if_changed(label, text.c_str());
}

void CheeseScreen::onSquish(lv_event_t *e) {
  auto *self = static_cast<CheeseScreen *>(lv_event_get_user_data(e));
  play_sound(Sound::MenuOpen);
  lv_anim_delete(self->cheeseman_, squish_cb);  // a tap mid-squish starts a new one
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, self->cheeseman_);
  lv_anim_set_exec_cb(&a, squish_cb);
  lv_anim_set_values(&a, 0, 100);
  lv_anim_set_duration(&a, SCENE.squish_ms);
  lv_anim_set_playback_duration(&a, SCENE.squish_ms * 2);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_start(&a);

  // Every third squish shakes the junk loose, if the faucet is ready.
  if (++self->squishes_ < SQUISHES_FOR_JUNK) return;
  self->squishes_ = 0;
  if (self->app().beetle().junkMsLeft(hal_millis()) == 0) self->app().claimJunk();
}

void CheeseScreen::onClaim(lv_event_t *e) {
  auto *self = static_cast<CheeseScreen *>(lv_event_get_user_data(e));
  if (self->app().beetle().cheeseMsLeft(hal_millis()) == 0) {
    self->app().claimCheese();
  } else {
    play_sound(Sound::Error);  // cooling down (replaces the press sound)
  }
}
