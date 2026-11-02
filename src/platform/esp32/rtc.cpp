#include "rtc.h"

#include <LovyanGFX.hpp>

#include "board_config.h"

static constexpr int I2C_PORT = 0;  // the touch controller's (lgfx_config.h)
static constexpr int ADDR = 0x51;
static constexpr uint32_t FREQ = 400000;
static constexpr uint8_t REG_SECONDS = 0x04;  // then minutes, hours, days, weekdays, months, years
static constexpr uint8_t OSCILLATOR_STOPPED = 0x80;  // in seconds: the time is not to be trusted

static int from_bcd(uint8_t v) { return (v >> 4) * 10 + (v & 15); }
static uint8_t to_bcd(int v) { return (uint8_t)((v / 10) << 4 | v % 10); }

// Days since 1970-01-01 for a civil date (newlib has no timegm; mktime would apply the timezone).
static int64_t days_from_civil(int y, int m, int d) {
  y -= m <= 2;
  int64_t era = (y >= 0 ? y : y - 399) / 400;
  int64_t yoe = y - era * 400;
  int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

bool rtc_read(time_t &utc) {
  uint8_t r[7];
  if (lgfx::i2c::readRegister(I2C_PORT, ADDR, REG_SECONDS, r, sizeof(r), FREQ).has_error()) return false;
  if (r[0] & OSCILLATOR_STOPPED) return false;
  int year = 2000 + from_bcd(r[6]), month = from_bcd(r[5] & 0x1f), day = from_bcd(r[3] & 0x3f);
  if (month < 1 || month > 12 || day < 1 || day > 31) return false;
  utc = (time_t)(days_from_civil(year, month, day) * 86400 + from_bcd(r[2] & 0x3f) * 3600 +
                 from_bcd(r[1] & 0x7f) * 60 + from_bcd(r[0] & 0x7f));
  return true;
}

void rtc_write(time_t utc) {
  struct tm t;
  gmtime_r(&utc, &t);
  // Writing the seconds also clears OSCILLATOR_STOPPED.
  uint8_t w[8] = {REG_SECONDS,          to_bcd(t.tm_sec),        to_bcd(t.tm_min),
                  to_bcd(t.tm_hour),    to_bcd(t.tm_mday),       (uint8_t)t.tm_wday,
                  to_bcd(t.tm_mon + 1), to_bcd(t.tm_year % 100)};
  lgfx::i2c::transactionWrite(I2C_PORT, ADDR, w, sizeof(w), FREQ);
}
