#include "hal/hal.h"

#include <CommonCrypto/CommonDigest.h>
#include <SDL.h>
#include <lvgl.h>
#include <mach-o/dyld.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "board_config.h"
#include "screenshot.h"

static constexpr int ZOOM = 2;  // the real screen is ~1.7"; 1:1 is tiny on a laptop display

void hal_play_tune(const HalNote *notes, size_t count) {
  uint32_t ms = 0;
  for (size_t i = 0; i < count; i++) ms += notes[i].ms;
  hal_log("[hal] tune: %u notes, %u ms (no buzzer in the sim)\n", (unsigned)count, (unsigned)ms);
}
void hal_set_volume(int) {}
void hal_leds_show(const uint8_t *, size_t) {}
int hal_battery_percent() { return 80; }  // something to lay out
bool hal_usb_connected() { return true; }
std::string hal_memory_report() { return "the sim's"; }

// A black veil over everything (even the transition curtain) stands in for the backlight.
void hal_set_brightness(int percent) {
  static lv_obj_t *veil = nullptr;
  if (!veil) {
    veil = lv_obj_create(lv_layer_sys());
    lv_obj_remove_style_all(veil);
    lv_obj_set_size(veil, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(veil, lv_color_black(), 0);
    lv_obj_remove_flag(veil, LV_OBJ_FLAG_CLICKABLE);
  }
  lv_obj_set_style_bg_opa(veil, (lv_opa_t)((100 - percent) * 255 / 100), 0);
}

void hal_display_sleep(bool sleep) {
  if (sleep) hal_set_brightness(0);  // the veil goes fully black; App restores the brightness
}

bool hal_light_sleep(uint32_t, HalWake &) { return false; }  // a laptop doesn't sleep for us

void hal_doze(bool) {}

void hal_init() {
  // Ctrl+C or closing the window: SDL posts SDL_QUIT, LVGL's driver shuts LVGL down and calls
  // exit(0). Static destructors (the App's screens) would then delete LVGL objects that no longer
  // exist and crash. This handler runs first (atexit order is reversed) and ends the process
  // without them; the OS frees everything.
  atexit([] {
    hal_log("[sim] bye\n");
    fflush(nullptr);
    _exit(0);
  });
  lv_display_t *disp = lv_sdl_window_create(SCREEN_W, SCREEN_H);  // also sets the LVGL tick
  lv_sdl_window_set_zoom(disp, ZOOM);
  lv_sdl_window_set_title(disp, "BeetleBoy32 sim");
  lv_sdl_mouse_create();  // mouse = touch
  // Unused, but must exist before the app's button keypad: LVGL's SDL driver hands key events to
  // the first keypad indev it finds and assumes it's its own (otherwise: crash on any key press).
  lv_sdl_keyboard_create();
  screenshot_init();
}

uint32_t hal_millis() { return SDL_GetTicks(); }
void hal_delay(uint32_t ms) { SDL_Delay(ms); }

void hal_restart() {
  char path[4096];
  uint32_t size = sizeof(path);
  fflush(nullptr);
  if (_NSGetExecutablePath(path, &size) == 0) execl(path, path, (char *)nullptr);
  _exit(1);
}
uint32_t hal_random() { return arc4random(); }
void hal_sha256(const void *data, size_t len, uint8_t out[32]) { CC_SHA256(data, (CC_LONG)len, out); }
void hal_set_thread_stack(uint32_t) {}
uint16_t hal_device_id() { return 0xBEE7; }

void hal_update() {}

void hal_set_timezone(const char *posix_tz) {  // the sim shows the chosen zone too
  setenv("TZ", posix_tz, 1);
  tzset();
}

bool hal_set_clock(int64_t) { return false; }  // the Mac's clock is right; never touch it

int64_t hal_epoch_ms() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

// BB_HOUR=0..23 overrides the hour, to preview time-of-day backgrounds.
bool hal_local_time(struct tm *out) {
  time_t now = time(nullptr);
  localtime_r(&now, out);
  if (const char *hour = getenv("BB_HOUR")) out->tm_hour = atoi(hour);
  return true;
}

void hal_log(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
  fflush(stdout);
}
