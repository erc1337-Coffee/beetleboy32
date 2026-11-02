#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <time.h>

// Platform primitives, implemented once per target in src/platform/<target>/hal.cpp.

void hal_init();  // after lv_init(): LVGL tick, display, touch/mouse input, clock from the RTC
void hal_update();  // once per main loop (board: saves NTP-synced time to the RTC)
uint32_t hal_millis();
void hal_delay(uint32_t ms);
[[noreturn]] void hal_restart();  // the board reboots; the sim starts itself over
uint32_t hal_random();  // hardware RNG (ESP32: true random only while the radio is on)
uint16_t hal_device_id();  // stable per device (low bits of the MAC on the board)
bool hal_local_time(struct tm *out);  // false until the clock has been set
int64_t hal_epoch_ms();  // wall clock, UTC; 0 until the clock has been set
void hal_set_timezone(const char *posix_tz);  // for hal_local_time(), e.g. "PST8PDT,M3.2.0,M11.1.0"
// Sets the wall clock (and the board's RTC). False where the platform owns the clock (the sim).
bool hal_set_clock(int64_t epoch_ms);
void hal_sha256(const void *data, size_t len, uint8_t out[32]);
void hal_set_brightness(int percent);  // backlight, 0-100 (the sim dims its window instead)
void hal_display_sleep(bool sleep);  // panel + backlight off (the touch controller stays on); restore brightness after
// Plays notes on the buzzer in the background; a new tune cuts off the one playing. hz 0 = a rest.
// to_hz: the pitch glides from hz to it over the note (0: steady).
struct HalNote {
  uint16_t hz, ms;
  uint16_t to_hz = 0;
};
void hal_play_tune(const HalNote *notes, size_t count);  // the sim only logs it
void hal_set_volume(int percent);  // buzzer, 0 (silent) to 100
// Sets the RGB LEDs (LED_COUNT, board_config.h): 3 bytes per LED (red, green, blue), first LED first.
// The sim ignores it.
void hal_leds_show(const uint8_t *rgb, size_t count);
// The battery's charge, 0-100 (from its voltage: a rough guide), or -1 without a reading. The sim: 80.
int hal_battery_percent();
// A USB host (a computer) is connected. A plain charger isn't seen: the board has no way to sense
// USB power itself. The sim: true.
bool hal_usb_connected();
// Screen off ("dozing"): touches read less often. The sim: nothing.
void hal_doze(bool on);
// Light sleep for up to `max_ms`, until then or a touch. Returns false where unsupported (the
// sim) or refused (a wake pin already active), without sleeping.
enum class HalWake { Timer, Input };
bool hal_light_sleep(uint32_t max_ms, HalWake &wake);
std::string hal_memory_report();  // free internal RAM and its largest block, for a log line
void hal_set_thread_stack(uint32_t bytes);  // for std::threads started after this call (ESP32 default: 3KB; on core 0)
void hal_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
