#pragma once

#include <lvgl.h>

#include <memory>
#include <vector>

#include "ui/screen.h"

// Stack of screens; the top one is visible and receives input.
// Navigation calls are queued and applied in update(), so a screen can safely
// navigate (and so destroy itself) from inside its own callbacks.
// Switches fade to black and back in (input is ignored meanwhile), unless asked to cut.

enum class Transition { Fade, Cut };
class ScreenManager {
 public:
  void begin(lv_indev_t *keypad) { keypad_ = keypad; }

  void push(std::unique_ptr<Screen> screen);
  void replace(std::unique_ptr<Screen> screen);  // swap the top screen
  void reset(std::unique_ptr<Screen> screen, Transition transition = Transition::Fade);  // clear, show this
  void pop();  // never closes the last screen
  void rebuildHidden();  // screens under the top rebuild when shown again (e.g. after a theme change)
  bool dispatch(const InputEvent &event);  // offers the event to the top screen

  void update();
  Screen *top() const { return stack_.empty() ? nullptr : stack_.back().get(); }

 private:
  enum class Op { Push, Replace, Reset, Pop };
  struct Pending {
    Op op;
    std::unique_ptr<Screen> screen;
    Transition transition = Transition::Fade;
  };
  enum class Fade { None, Out, In };

  void apply(Pending &p);
  void showTop();
  void startFade(Pending p);
  void stepFade();

  lv_indev_t *keypad_ = nullptr;
  std::vector<std::unique_ptr<Screen>> stack_;
  std::vector<Pending> pending_;
  Fade fade_ = Fade::None;
  uint32_t fadeStart_ = 0;
  Pending fading_;  // applied once the screen is black
  lv_obj_t *curtain_ = nullptr;  // black, on LVGL's top layer
};
