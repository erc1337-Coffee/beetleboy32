#include "hal/buttons.h"

#include <SDL.h>

// Debug keys (the board has no buttons): Esc = Cancel, Enter = OK, Tab or Right = Next. LVGL's SDL
// driver pumps the event queue, so SDL's keyboard state is already current here.

void buttons_begin() {}

uint8_t buttons_read() {
  const Uint8 *k = SDL_GetKeyboardState(nullptr);
  uint8_t held = 0;
  if (k[SDL_SCANCODE_ESCAPE]) held |= BTN_CANCEL;
  if (k[SDL_SCANCODE_RETURN] || k[SDL_SCANCODE_KP_ENTER]) held |= BTN_OK;
  if (k[SDL_SCANCODE_RIGHT] || k[SDL_SCANCODE_TAB]) held |= BTN_NEXT;
  return held;
}
