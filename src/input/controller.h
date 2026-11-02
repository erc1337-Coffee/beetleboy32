#pragma once

#include <lvgl.h>
#include <stdint.h>

#include <functional>

#include "hal/buttons.h"

enum class InputSource : uint8_t { Touch, Button };

enum class InputAction : uint8_t {
  Tap,        // touch: short tap outside any widget
  LongPress,  // touch: held outside any widget
  SwipeLeft,
  SwipeRight,
  SwipeUp,
  SwipeDown,
  Confirm,  // button OK, on a screen without widgets
  Next,     // button Next, on a screen without widgets
  Back,     // button Cancel (short press), or the on-screen Back button
  Reset,    // button Cancel held for LONG_PRESS_MS
  Hold,     // touch: held TOUCH_HOLD_MS anywhere, widgets included (the hidden picture)
};

struct InputEvent {
  InputSource source;
  InputAction action;
  uint8_t button = 0;         // Button bit, for InputSource::Button
  lv_point_t point = {0, 0};  // screen position, for InputSource::Touch
};

// The single entry point for user input: physical buttons and the touchscreen.
//  - Widgets (anything focusable: buttons, menu tiles) handle their own clicks, so a widget's click
//    handler runs the same whether it was tapped or pressed with OK; Next moves focus between them.
//  - Everything else becomes an InputEvent passed to `handler` (the App, which offers it to the top
//    screen first). On a screen with no widgets, OK and Next become Confirm/Next events too.
class Controller {
 public:
  using Handler = std::function<void(const InputEvent &)>;

  void begin(Handler handler);  // after hal_init(): needs the touch input device
  void update();                // once per loop: polls the buttons
  lv_indev_t *keypad() const { return keypad_; }
  // The finger and buttons now down do nothing until lifted (e.g. what woke the screen), nor do
  // buttons pressed in the next moment (the press that woke it may not be debounced yet).
  void swallowInput();
  // Screen off: the buttons only wake it. darkPress() is true once after one goes down.
  void setDark(bool dark) {
    dark_ = dark;
    darkPress_ = false;  // a press from before doesn't count
  }
  bool darkPress() {
    const bool pressed = darkPress_;
    darkPress_ = false;
    return pressed;
  }

 private:
  static void readKeypad(lv_indev_t *indev, lv_indev_data_t *data);
  static void onTouch(lv_event_t *e);
  bool screenHasWidgets() const;
  void emit(const InputEvent &event);
  void emitButton(uint8_t button, InputAction action);

  Handler handler_;
  lv_indev_t *keypad_ = nullptr;
  lv_indev_t *touch_ = nullptr;
  uint8_t held_ = 0;      // pressed and not swallowed
  uint8_t swallowed_ = 0;  // ignored until released
  uint32_t swallowUntil_ = 0;
  bool dark_ = false, darkPress_ = false;
  uint32_t lastKey_ = LV_KEY_ENTER;
  uint32_t cancelDownAt_ = 0;
  bool cancelWasLong_ = false;
  uint32_t touchDownAt_ = 0;
  bool touchDown_ = false;
  bool holdFired_ = false;
};
