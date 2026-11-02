#pragma once

#include <time.h>

// The board's PCF85063 real-time clock (I2C 0x51, on the touch controller's bus). Keeps UTC while
// the board is off (on its backup supply), so the time is right before Wi-Fi and NTP.
// Call after the display (and so the I2C bus) is initialized; main thread only (shares the bus
// with touch reads).
bool rtc_read(time_t &utc);  // false if it lost power since it was last set, or isn't there
void rtc_write(time_t utc);
