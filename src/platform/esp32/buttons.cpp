#include "hal/buttons.h"

#include <Arduino.h>

#include "board_config.h"

static constexpr uint32_t DEBOUNCE_MS = 25;

struct ButtonPin {
  Button id;
  uint8_t pin;
  bool stable;  // debounced "pressed" state
  bool raw;
  uint32_t changedAt;
};

static ButtonPin pins[] = {
    {BTN_CANCEL, PIN_BTN_CANCEL},
    {BTN_OK, PIN_BTN_OK},
#ifdef PIN_BTN_NEXT
    {BTN_NEXT, PIN_BTN_NEXT},
#endif
};

void buttons_begin() {
  for (auto &b : pins) pinMode(b.pin, INPUT_PULLUP);
}

uint8_t buttons_read() {
  uint32_t now = millis();
  uint8_t held = 0;
  for (auto &b : pins) {
    bool raw = digitalRead(b.pin) == LOW;  // active low
    if (raw != b.raw) {
      b.raw = raw;
      b.changedAt = now;
    } else if (raw != b.stable && now - b.changedAt >= DEBOUNCE_MS) {
      b.stable = raw;
    }
    if (b.stable) held |= b.id;
  }
  return held;
}
