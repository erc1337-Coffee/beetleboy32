#include <Arduino.h>

#include "board_config.h"
#include "hal/hal.h"

// WS2812B timing, in 100 ns ticks: a 1 is long high then short low, a 0 the other way round.
static constexpr uint32_t T_SHORT = 4, T_LONG = 8;
static constexpr size_t BITS = LED_COUNT * 24;

void hal_leds_show(const uint8_t *rgb, size_t count) {
  static rmt_obj_t *rmt = nullptr;
  static bool failed = false;
  if (!rmt && !failed) {
    // Room for every bit at once (S3: 48 per block), so nothing refills mid-frame under Wi-Fi.
    rmt = rmtInit(PIN_LEDS, RMT_TX_MODE, RMT_MEM_192);
    if (!rmt) {
      failed = true;
      hal_log("[leds] driver didn't start\n");
      return;
    }
    rmtSetTick(rmt, 100);
  }
  if (!rmt) return;
  static rmt_data_t bits[BITS];
  size_t n = 0;
  for (size_t led = 0; led < LED_COUNT; led++) {
    const uint8_t *c = led < count ? rgb + led * 3 : nullptr;
    const uint8_t grb[3] = {c ? c[1] : (uint8_t)0, c ? c[0] : (uint8_t)0, c ? c[2] : (uint8_t)0};  // the chip's order
    for (uint8_t byte : grb) {
      for (int bit = 7; bit >= 0; bit--) {
        const bool one = byte >> bit & 1;
        bits[n].level0 = 1;
        bits[n].duration0 = one ? T_LONG : T_SHORT;
        bits[n].level1 = 0;
        bits[n].duration1 = one ? T_SHORT : T_LONG;
        n++;
      }
    }
  }
  rmtWriteBlocking(rmt, bits, BITS);  // ~120 us for 4 LEDs
}
