#include "media/lights.h"

#include <math.h>
#include <string.h>

#include <iterator>

#include "hal/hal.h"

static constexpr uint32_t FRAME_MS = 20;  // ~50 frames a second
// Animation speeds, in ms.
static constexpr uint32_t COLOR_STEP_MS = 1000, EACH_STEP_MS = 600, GRADIENT_MS = 3000, BREATHE_MS = 1500;
static constexpr uint32_t CHASE_STEP_MS = 140, ALTERNATE_MS = 1000;
static constexpr int ACTIVE_PERCENT = 100;  // one-shots and the busy chase
// The case is translucent green: everything stays between these.
static constexpr uint8_t WHITE[3] = {255, 255, 255}, GREEN[3] = {0, 255, 40}, CYAN[3] = {0, 230, 200};

static const char *const NAMES[] = {
    "WHITE", "GREEN", "CYAN", "COLOR TEST", "EACH LED", "GRADIENT", "BREATHE", "CHASE", "FLICKER", "ALTERNATE", "SPARKLE",
};
static_assert(std::size(NAMES) == (size_t)LightEffect::Count, "one name per LightEffect, in order");
const char *light_effect_name(LightEffect effect) { return NAMES[(int)effect]; }

static void set(uint8_t c[3], const uint8_t color[3]) { memcpy(c, color, 3); }

static void blend(uint8_t c[3], const uint8_t from[3], const uint8_t to[3], uint32_t k) {  // k 0-255: from to `to`
  for (int i = 0; i < 3; i++) c[i] = (uint8_t)((from[i] * (255 - k) + to[i] * k) / 255);
}

static void scale(uint8_t c[3], uint32_t k) {  // k 0-255
  for (int i = 0; i < 3; i++) c[i] = (uint8_t)(c[i] * k / 255);
}

// 0-255 around the loop white, green, cyan, white.
static void palette(uint8_t c[3], uint8_t p) {
  const uint32_t x = (p % 85) * 3;
  if (p < 85) blend(c, WHITE, GREEN, x);
  else if (p < 170) blend(c, GREEN, CYAN, x);
  else blend(c, CYAN, WHITE, x);
}

void Lights::setGlow(bool on, int percent) {
  dirty_ |= on != glowOn_ || percent != glowPercent_;
  glowOn_ = on;
  glowPercent_ = percent;
}

void Lights::setBusy(bool busy) {
  if (busy && !busy_) busyAt_ = hal_millis();
  dirty_ |= busy != busy_;
  busy_ = busy;
}

void Lights::play(LightEffect effect, uint32_t ms) {
  oneShot_ = effect;
  oneShotAt_ = hal_millis();
  oneShotMs_ = ms;
  for (float &s : sparks_) s = 0;
  dirty_ = true;
}

void Lights::startDemo(LightEffect effect, int percent) {
  if (!demo_ || effect != demoEffect_) demoAt_ = hal_millis();
  if (effect != demoEffect_) for (float &s : sparks_) s = 0;
  demo_ = true;
  demoEffect_ = effect;
  demoPercent_ = percent;
  dirty_ = true;
}

void Lights::stopDemo() {
  demo_ = false;
  demoEffect_ = LightEffect::Count;
  dirty_ = true;
}

void Lights::update() {
  const uint32_t now = hal_millis();
  if (!dirty_ && now - shownAt_ < FRAME_MS) return;
  dirty_ = false;
  shownAt_ = now;
  if (oneShot_ != LightEffect::Count && now - oneShotAt_ >= oneShotMs_) oneShot_ = LightEffect::Count;

  uint8_t rgb[LED_COUNT][3] = {};
  int percent = ACTIVE_PERCENT;
  if (demo_) {  // the demo's own choice, even with the lights off (Count: dark)
    if (demoEffect_ != LightEffect::Count) render(demoEffect_, now - demoAt_, rgb);
    percent = demoPercent_;
  } else if (glowOn_) {  // off: dark
    if (oneShot_ != LightEffect::Count) {
      render(oneShot_, now - oneShotAt_, rgb);
    } else if (busy_) {
      render(LightEffect::Chase, now - busyAt_, rgb);
    } else {
      render(LightEffect::Green, 0, rgb);
      percent = glowPercent_;
    }
  }
  for (auto &c : rgb)
    for (uint8_t &v : c) v = (uint8_t)(v * percent / 100);
  if (memcmp(rgb, shown_, sizeof(rgb)) == 0) return;
  memcpy(shown_, rgb, sizeof(rgb));
  hal_leds_show(&rgb[0][0], LED_COUNT);
}

void Lights::render(LightEffect effect, uint32_t t, uint8_t rgb[LED_COUNT][3]) {
  auto random = [this] {  // xorshift: plenty for flicker
    seed_ ^= seed_ << 13;
    seed_ ^= seed_ >> 17;
    seed_ ^= seed_ << 5;
    return seed_;
  };
  for (int i = 0; i < LED_COUNT; i++) {
    uint8_t *c = rgb[i];
    switch (effect) {
      case LightEffect::White:
        set(c, WHITE);
        break;
      case LightEffect::Green:
        set(c, GREEN);
        break;
      case LightEffect::Cyan:
        set(c, CYAN);
        break;
      case LightEffect::Colors: {  // white, green, cyan: every LED together
        static const uint8_t *const STEPS[] = {WHITE, GREEN, CYAN};
        set(c, STEPS[t / COLOR_STEP_MS % 3]);
        break;
      }
      case LightEffect::Each:  // one at a time, first to last: shows the wiring order
        if ((int)(t / EACH_STEP_MS % LED_COUNT) == i) set(c, WHITE);
        break;
      case LightEffect::Gradient:  // the palette, moving along the LEDs
        palette(c, (uint8_t)(t * 256 / GRADIENT_MS + i * 256 / LED_COUNT));
        break;
      case LightEffect::Breathe: {  // a cyan-green pulse
        const float k = (1 - cosf(2 * (float)M_PI * (float)(t % BREATHE_MS) / BREATHE_MS)) / 2;
        blend(c, GREEN, CYAN, 128);
        scale(c, (uint32_t)(255 * k));
        break;
      }
      case LightEffect::Chase: {  // a green dot with a fading tail
        const int head = (int)(t / CHASE_STEP_MS % LED_COUNT);
        const int behind = (head - i + LED_COUNT) % LED_COUNT;
        set(c, GREEN);
        scale(c, 255u >> (2 * behind));
        break;
      }
      case LightEffect::Flicker: {  // each LED flickers on its own, green to cyan
        const uint32_t r = random();
        blend(c, GREEN, CYAN, (r >> 8) % 256);
        scale(c, 150 + r % 106);
        break;
      }
      case LightEffect::Alternate: {  // the first half flashes green twice, then the second half cyan
        const uint32_t p = t % ALTERNATE_MS;
        const bool first = i < LED_COUNT / 2, green_turn = p < ALTERNATE_MS / 2;
        const bool on = p % (ALTERNATE_MS / 4) < ALTERNATE_MS / 8;
        if (on && first == green_turn) set(c, green_turn ? GREEN : CYAN);
        break;
      }
      case LightEffect::Sparkle: {  // white flashes at random over a dim green, fading back
        sparks_[i] *= 0.85f;
        if (random() % 40 == 0) sparks_[i] = 1;
        uint8_t base[3];
        set(base, GREEN);
        scale(base, 30);
        blend(c, base, WHITE, (uint32_t)(255 * sparks_[i]));
        break;
      }
      case LightEffect::Count:
        break;
    }
  }
}
