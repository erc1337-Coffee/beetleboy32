#pragma once

#include <lvgl.h>
#include <stdint.h>

#include <functional>
#include <vector>

// A small popup at the top of the screen, over every screen, like an achievement badge: an icon (or
// a picture, such as a PFP) left of a short line of text, wrapped and centered if it doesn't fit.
// A new toast replaces the one showing. Main thread only.
//  - Without buttons: shows for `duration_ms`, then fades out; taps go through to the screen.
//  - With buttons (a row under the text): stays until one is pressed (which closes it, then runs its
//    action), hide_toast(), or `duration_ms` if not 0; taps elsewhere don't reach the screen below.
struct ToastButton {
  const char *label;
  std::function<void()> onPress;  // none: plain text in the row, not a button
};
void show_toast(const void *icon, const char *text, uint32_t duration_ms = 2000,
                std::vector<ToastButton> buttons = {});
void hide_toast();
