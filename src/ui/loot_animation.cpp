#include "ui/loot_animation.h"

#include <math.h>
#include <stdint.h>

#include <algorithm>

#include "board_config.h"
#include "fonts/fonts.h"
#include "hal/hal.h"
#include "ui/theme.h"

// Timeline, in ms, before randomness: each item gets its own variation (vary()), and items start
// one after the other, so nothing moves in lockstep.
static constexpr int32_t GROW_MS = 950, WIGGLE_MS = 1200, FALL_MS = 800;
static constexpr int32_t GROW_SPREAD = 150, WIGGLE_SPREAD = 200, FALL_SPREAD = 150;
static constexpr int32_t STAGGER_MS = 170, STAGGER_SPREAD = 70;  // between consecutive items
static constexpr int WIGGLES = 3;
static constexpr int32_t WIGGLE_PCT = 12, WIGGLE_PCT_SPREAD = 4;  // +-% size, fading over the wiggles
static constexpr int32_t FALL_PX = 320;                            // far enough to leave the screen
static constexpr int32_t HOP_PX = 10, HOP_SPREAD = 6;              // a little jump before dropping
static constexpr int32_t DRIFT_PX = 18;                            // sideways while falling, +-
static constexpr int32_t SPIN_DEG = 25;                            // tumble while falling, +-
static constexpr int32_t MAIN_TURNS = 2;  // the main item spins in while growing, either way, landing upright
static constexpr float PI = 3.14159265f;

static constexpr int32_t MAIN_SIZE = 80;  // px, the main icon (App::ITEM_ICON_SIZE fits icons to this)
static constexpr int32_t SIDE_OFFSET = 72, SIDE_STEP = 48;  // secondary icons, from the center
static constexpr lv_opa_t GLARE_OPA = LV_OPA_60;

static constexpr int32_t MESSAGE_HOLD_MS = 1000, MESSAGE_FADE_MS = 500;
// hurry_loot(): an item plays the rest of its timeline in this (a fraction of it for what's left), a
// message fades out in HURRY_FADE_MS.
static constexpr int32_t HURRY_MS = 400, HURRY_FADE_MS = 200;
// A boxed message: dark text on a white, nearly square box (readable over a busy picture).
static constexpr uint32_t BOX_BG = 0xffffff, BOX_INK = theme::INK, BOX_BORDER = theme::INK;
static constexpr int32_t BOX_RADIUS = 2, BOX_BORDER_W = 2, BOX_PAD_HOR = 10, BOX_PAD_VER = 5;

// Rain: drops start over RAIN_SPAWN_MS, each falling for RAIN_FALL_MS, give or take RAIN_FALL_SPREAD.
static constexpr int RAIN_DROPS = 26;
static constexpr int32_t RAIN_SPAWN_MS = 1600, RAIN_FALL_MS = 1200, RAIN_FALL_SPREAD = 300;
static constexpr int RAIN_MIN_SCALE = 2, RAIN_MAX_SCALE = 3;  // whole factors of the icon: crisp pixels
static constexpr int32_t RAIN_DRIFT_PX = 14, RAIN_SPIN_DEG = 90;

static void delete_when_done(lv_anim_t *done) { lv_obj_delete_async(static_cast<lv_obj_t *>(done->var)); }

static int32_t vary(int32_t base, int32_t spread) {
  return base - spread + (int32_t)(hal_random() % (uint32_t)(2 * spread + 1));
}

// One item's own take on the timeline.
struct Motion {
  uint32_t rest;  // resting scale, 256 = 1:1
  int32_t grow_ms, wiggle_ms, fall_ms;
  float wiggle;                 // size swing, fraction
  int32_t glare_at, glare_ms;   // within the wiggle
  int32_t hop, drift, spin;     // px, px, 0.1 degree
  int32_t turn;                 // 0.1 degree, spun off while growing (0: no spin)
  bool started = false;         // its delay is over: it's on screen
  int32_t at = 0;               // ms into the timeline, as last drawn
  int32_t total() const { return grow_ms + wiggle_ms + fall_ms; }
};

static Motion random_motion(uint32_t rest, bool secondary) {
  Motion m;
  m.rest = rest;
  m.grow_ms = vary(GROW_MS, GROW_SPREAD);
  m.wiggle_ms = vary(WIGGLE_MS, WIGGLE_SPREAD);
  m.fall_ms = vary(FALL_MS, FALL_SPREAD);
  m.wiggle = vary(WIGGLE_PCT, WIGGLE_PCT_SPREAD) / 100.0f;
  m.glare_ms = m.wiggle_ms * vary(70, 15) / 100;
  m.glare_at = (int32_t)(hal_random() % (uint32_t)(m.wiggle_ms - m.glare_ms + 1));
  m.hop = vary(HOP_PX, HOP_SPREAD);
  m.drift = vary(0, DRIFT_PX);
  m.spin = vary(0, SPIN_DEG) * 10;
  m.turn = secondary ? 0 : (hal_random() % 2 ? 1 : -1) * MAIN_TURNS * 3600;
  return m;
}

// Sizes come from the image, not from object coordinates: those aren't laid out yet on the first frame.
struct Geometry {
  int32_t w, h;    // image
  int32_t dw, dh;  // drawn at rest = the item's box
  int32_t band_w;
};

static Geometry geometry(const lv_image_dsc_t *image, uint32_t rest) {
  Geometry g;
  g.w = image->header.w;
  g.h = image->header.h;
  g.dw = g.w * rest / LV_SCALE_NONE;
  g.dh = g.h * rest / LV_SCALE_NONE;
  g.band_w = g.dw / 3 + 1;
  return g;
}

static void step(void *var, int32_t t) {
  auto *item = static_cast<lv_obj_t *>(var);
  lv_obj_t *icon = lv_obj_get_child(item, 0), *band = lv_obj_get_child(item, 1);
  lv_obj_t *shine = lv_obj_get_child(band, 0);
  Motion &m = *static_cast<Motion *>(lv_obj_get_user_data(item));
  m.at = t;
  const Geometry g = geometry(static_cast<const lv_image_dsc_t *>(lv_image_get_src(icon)), m.rest);

  float scale = 1, x = 0, y = 0, spin = 0;
  int32_t band_x = -g.band_w;
  if (t < m.grow_ms) {
    float p = (float)t / m.grow_ms - 1;  // ease-out-back: overshoots a bit, then settles at 1
    scale = 1 + 2.70158f * p * p * p + 1.70158f * p * p;
    spin = m.turn * p * p * p;  // ease-out-cubic (p goes -1..0): fast at first, slowing to a stop upright
  } else if (t < m.grow_ms + m.wiggle_ms) {
    int32_t w = t - m.grow_ms;
    float p = (float)w / m.wiggle_ms;
    scale = 1 + m.wiggle * (1 - 0.5f * p) * sinf(p * WIGGLES * 2 * PI);  // calming down
    if (w >= m.glare_at && w < m.glare_at + m.glare_ms) {
      float q = (float)(w - m.glare_at) / m.glare_ms;
      band_x = (int32_t)(-g.band_w + q * (g.dw + g.band_w));
    }
  } else {
    float p = (float)(t - m.grow_ms - m.wiggle_ms) / m.fall_ms;
    y = p * p * FALL_PX - m.hop * 4 * p * (1 - p);  // hop up a little, then accelerate down
    x = m.drift * p;
    spin = m.spin * p * p;
  }

  uint32_t s = (uint32_t)(m.rest * scale);
  if (s < 1) s = 1;
  lv_image_set_scale(icon, s);
  lv_image_set_scale(shine, s);
  lv_image_set_rotation(icon, (int32_t)spin);
  lv_image_set_rotation(shine, (int32_t)spin);
  lv_obj_set_x(band, band_x);
  // The white copy sits where the icon is (the band starts dh/2 above the item).
  lv_obj_set_pos(shine, (g.dw - g.w) / 2 - band_x, (g.dh - g.h) / 2 + g.dh / 2);
  lv_obj_set_style_translate_x(item, (int32_t)x, 0);
  lv_obj_set_style_translate_y(item, (int32_t)y, 0);
}

// An item is a box the size of the drawn icon, holding:
//   child 0: the icon (scaled around its center)
//   child 1: the glare band, a narrow strip that clips child 0 of its own: a white copy of the icon,
//            kept aligned with the real one, so only the icon's own pixels light up.
// Its user data is its Motion (freed with it). Returns when it's gone, in ms from now.
static uint32_t make_item(lv_obj_t *parent, const lv_image_dsc_t *image, bool secondary, lv_point_t at,
                      uint32_t delay_ms) {
  const int32_t w = image->header.w, h = image->header.h;
  const int32_t longest = w > h ? w : h;
  // Fitted icons show 1:1; tiny ones (a fallback) grow by a whole factor to stay crisp.
  uint32_t rest = longest * 2 <= MAIN_SIZE ? LV_SCALE_NONE * (MAIN_SIZE / longest) : LV_SCALE_NONE;
  if (secondary) rest /= 2;
  const Geometry g = geometry(image, rest);
  const int32_t dw = g.dw, dh = g.dh;

  lv_obj_t *item = lv_obj_create(parent);
  lv_obj_remove_style_all(item);
  lv_obj_set_size(item, dw, dh);
  lv_obj_set_pos(item, at.x - dw / 2, at.y - dh / 2);
  lv_obj_add_flag(item, LV_OBJ_FLAG_OVERFLOW_VISIBLE);  // wiggling makes it bigger than its box
  lv_obj_remove_flag(item, LV_OBJ_FLAG_CLICKABLE);
  auto *motion = new Motion(random_motion(rest, secondary));
  lv_obj_set_user_data(item, motion);
  lv_obj_add_event_cb(
      item, [](lv_event_t *e) { delete static_cast<Motion *>(lv_event_get_user_data(e)); }, LV_EVENT_DELETE,
      motion);

  // Scaling draws around the image's center, so center the unscaled image in the box.
  lv_obj_t *icon = lv_image_create(item);
  lv_image_set_src(icon, image);
  lv_image_set_antialias(icon, false);
  lv_image_set_scale(icon, 1);  // nothing to see until its turn
  lv_obj_set_size(icon, w, h);
  lv_obj_set_pos(icon, (dw - w) / 2, (dh - h) / 2);

  lv_obj_t *band = lv_obj_create(item);
  lv_obj_remove_style_all(band);
  lv_obj_set_size(band, g.band_w, dh * 2);
  lv_obj_set_pos(band, -g.band_w, -dh / 2);
  lv_obj_t *shine = lv_image_create(band);
  lv_image_set_src(shine, image);
  lv_image_set_antialias(shine, false);
  lv_image_set_scale(shine, 1);
  lv_obj_set_size(shine, w, h);
  lv_obj_set_style_image_recolor(shine, lv_color_white(), 0);
  lv_obj_set_style_image_recolor_opa(shine, LV_OPA_COVER, 0);
  lv_obj_set_style_image_opa(shine, GLARE_OPA, 0);

  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, item);
  lv_anim_set_exec_cb(&a, step);
  lv_anim_set_values(&a, 0, motion->total());
  lv_anim_set_duration(&a, motion->total());
  lv_anim_set_delay(&a, delay_ms);
  lv_anim_set_path_cb(&a, lv_anim_path_linear);  // the phases ease themselves
  lv_anim_set_start_cb(&a, [](lv_anim_t *started) {
    static_cast<Motion *>(lv_obj_get_user_data(static_cast<lv_obj_t *>(started->var)))->started = true;
  });
  lv_anim_set_completed_cb(&a, delete_when_done);
  lv_anim_start(&a);
  return delay_ms + motion->total();
}

uint32_t play_loot(lv_obj_t *parent, const std::vector<LootIcon> &icons, lv_point_t center) {
  // The main item first, then the drops, each a beat after the previous one.
  std::vector<LootIcon> order;
  for (const LootIcon &icon : icons)
    if (icon.image && !icon.secondary) order.push_back(icon);
  for (const LootIcon &icon : icons)
    if (icon.image && icon.secondary) order.push_back(icon);

  int left = 0, right = 0;
  uint32_t delay = 0, end = 0;
  for (const LootIcon &icon : order) {
    lv_point_t at = center;
    if (icon.secondary) {
      bool on_left = left <= right;
      int slot = on_left ? left++ : right++;
      at.x += (on_left ? -1 : 1) * (SIDE_OFFSET + slot * SIDE_STEP);
    }
    end = std::max(end, make_item(parent, icon.image, icon.secondary, at, delay));
    delay += vary(STAGGER_MS, STAGGER_SPREAD);
  }
  return end;
}

struct Drop {
  int32_t x, size;      // px: column, drawn size
  int32_t drift, spin;  // px sideways, 0.1 degree, over the fall
};

static void rain_step(void *var, int32_t t) {  // t: 0..1000 of this drop's fall
  auto *img = static_cast<lv_obj_t *>(var);
  const Drop &d = *static_cast<const Drop *>(lv_obj_get_user_data(img));
  float p = t / 1000.0f;
  float from = -d.size, to = SCREEN_H + d.size;          // fully out of view at both ends
  float y = from + (to - from) * (0.35f * p + 0.65f * p * p);  // already moving, then speeding up
  const lv_image_dsc_t *src = static_cast<const lv_image_dsc_t *>(lv_image_get_src(img));
  lv_obj_set_pos(img, d.x + (int32_t)(d.drift * p) - src->header.w / 2, (int32_t)y - src->header.h / 2);
  lv_image_set_rotation(img, (int32_t)(d.spin * p));
}

uint32_t play_rain(lv_obj_t *parent, const lv_image_dsc_t *icon) {
  uint32_t end = 0;
  for (int i = 0; i < RAIN_DROPS; i++) {
    int scale = RAIN_MIN_SCALE + (int)(hal_random() % (RAIN_MAX_SCALE - RAIN_MIN_SCALE + 1));
    auto *drop = new Drop{(int32_t)(hal_random() % SCREEN_W), (int32_t)icon->header.h * scale,
                          vary(0, RAIN_DRIFT_PX), vary(0, RAIN_SPIN_DEG) * 10};

    lv_obj_t *img = lv_image_create(parent);
    lv_image_set_src(img, icon);
    lv_image_set_scale(img, LV_SCALE_NONE * scale);
    lv_image_set_antialias(img, false);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);  // until its turn
    lv_obj_set_user_data(img, drop);
    lv_obj_add_event_cb(
        img, [](lv_event_t *e) { delete static_cast<Drop *>(lv_event_get_user_data(e)); }, LV_EVENT_DELETE,
        drop);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, img);
    lv_anim_set_exec_cb(&a, rain_step);
    lv_anim_set_values(&a, 0, 1000);
    const uint32_t fall = vary(RAIN_FALL_MS, RAIN_FALL_SPREAD), delay = hal_random() % RAIN_SPAWN_MS;
    end = std::max(end, delay + fall);
    lv_anim_set_duration(&a, fall);
    lv_anim_set_delay(&a, delay);
    lv_anim_set_start_cb(&a, [](lv_anim_t *started) {
      lv_obj_remove_flag(static_cast<lv_obj_t *>(started->var), LV_OBJ_FLAG_HIDDEN);
    });
    lv_anim_set_completed_cb(&a, delete_when_done);
    lv_anim_start(&a);
  }
  return end;
}

static void fade(void *var, int32_t opa) {
  lv_obj_set_style_opa(static_cast<lv_obj_t *>(var), (lv_opa_t)opa, 0);
}

// Runs `exec` on `obj` from `from` to `to` over `ms`, then deletes `obj`.
static void play_out(lv_obj_t *obj, lv_anim_exec_xcb_t exec, int32_t from, int32_t to, uint32_t ms) {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, obj);
  lv_anim_set_exec_cb(&a, exec);
  lv_anim_set_values(&a, from, to);
  lv_anim_set_duration(&a, ms > 0 ? ms : 1);
  lv_anim_set_completed_cb(&a, delete_when_done);
  lv_anim_start(&a);
}

void hurry_loot(lv_obj_t *parent) {
  // Backwards: deleting a child shifts the ones after it.
  for (int32_t i = (int32_t)lv_obj_get_child_count(parent) - 1; i >= 0; i--) {
    lv_obj_t *child = lv_obj_get_child(parent, i);
    if (lv_anim_get(child, step)) {
      const Motion &m = *static_cast<const Motion *>(lv_obj_get_user_data(child));
      if (!m.started) {  // never seen: just gone
        lv_obj_delete(child);
        continue;
      }
      const int32_t from = m.at, to = m.total();
      lv_anim_delete(child, step);
      play_out(child, step, from, to, (uint32_t)(HURRY_MS * (to - from) / to));
    } else if (lv_anim_get(child, fade)) {
      lv_anim_delete(child, fade);
      play_out(child, fade, lv_obj_get_style_opa(child, 0), LV_OPA_TRANSP, HURRY_FADE_MS);
    }
  }
}

void play_loot_message(lv_obj_t *parent, const char *text, lv_point_t center, bool boxed) {
  lv_obj_t *label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, &font_menlo_16, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(boxed ? BOX_INK : 0xffffff), 0);
  if (boxed) {
    lv_obj_set_style_bg_color(label, lv_color_hex(BOX_BG), 0);
    lv_obj_set_style_bg_opa(label, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(label, lv_color_hex(BOX_BORDER), 0);
    lv_obj_set_style_border_width(label, BOX_BORDER_W, 0);
    lv_obj_set_style_radius(label, BOX_RADIUS, 0);
    lv_obj_set_style_pad_hor(label, BOX_PAD_HOR, 0);
    lv_obj_set_style_pad_ver(label, BOX_PAD_VER, 0);
  }
  lv_obj_align(label, LV_ALIGN_CENTER, center.x - SCREEN_W / 2, center.y - SCREEN_H / 2);

  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, label);
  lv_anim_set_exec_cb(&a, fade);
  lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
  lv_anim_set_delay(&a, MESSAGE_HOLD_MS);
  lv_anim_set_duration(&a, MESSAGE_FADE_MS);
  lv_anim_set_completed_cb(&a, delete_when_done);
  lv_anim_start(&a);
}
