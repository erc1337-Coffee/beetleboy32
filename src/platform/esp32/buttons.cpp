#include "hal/buttons.h"

// The board has no buttons: only the simulator's debug keys use this interface.

void buttons_begin() {}

uint8_t buttons_read() { return 0; }
