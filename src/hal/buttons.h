#pragma once

#include <stdint.h>

// Buttons as a bitmask. The board has none (it reads nothing); the simulator's keys stand in, for debugging.
enum Button : uint8_t {
  BTN_CANCEL = 1 << 0,
  BTN_OK = 1 << 1,
  BTN_NEXT = 1 << 2,
};

void buttons_begin();
uint8_t buttons_read();  // debounced set of held buttons; call every loop
