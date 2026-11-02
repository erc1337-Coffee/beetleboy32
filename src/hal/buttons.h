#pragma once

#include <stdint.h>

// Physical buttons as a bitmask. Implemented per platform: GPIOs on the board, keyboard in the sim.
enum Button : uint8_t {
  BTN_CANCEL = 1 << 0,
  BTN_OK = 1 << 1,
  BTN_NEXT = 1 << 2,
};

void buttons_begin();
uint8_t buttons_read();  // debounced set of held buttons; call every loop
