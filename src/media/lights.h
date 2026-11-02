#pragma once

#include <stddef.h>
#include <stdint.h>

#include "board_config.h"

// The RGB LEDs (board_config.h). One effect shows at a time: a one-shot (play()) over the chase while
// a request is pending (setBusy()), over the always-on glow (setGlow()). The LEDs demo takes them
// over while it runs.
enum class LightEffect { White, Green, Cyan, Colors, Each, Gradient, Breathe, Chase, Flicker, Alternate, Sparkle, Count };
const char *light_effect_name(LightEffect effect);  // "WHITE"...

class Lights {
 public:
  void setGlow(bool on, int percent);  // the always-on green; off: no lights at all (the demo aside)
  void setBusy(bool busy);
  void play(LightEffect effect, uint32_t ms);  // at full brightness, then back to what was showing
  void startDemo(LightEffect effect, int percent);
  void stopDemo();
  void update();  // every loop: the next frame

 private:
  void render(LightEffect effect, uint32_t t, uint8_t rgb[LED_COUNT][3]);

  bool glowOn_ = true, busy_ = false, demo_ = false;
  int glowPercent_ = 25, demoPercent_ = 100;
  LightEffect oneShot_ = LightEffect::Count, demoEffect_ = LightEffect::Count;  // Count: none
  uint32_t oneShotAt_ = 0, oneShotMs_ = 0, busyAt_ = 0, demoAt_ = 0;
  uint32_t shownAt_ = 0;
  bool dirty_ = true;
  uint8_t shown_[LED_COUNT][3] = {};
  float sparks_[LED_COUNT] = {};
  uint32_t seed_ = 1;
};
