#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_pthread.h>
#include <esp_sleep.h>
#include <esp_sntp.h>
#include <driver/gpio.h>
#include <mbedtls/sha256.h>
#include <soc/usb_serial_jtag_reg.h>
#include <stdarg.h>

#include <algorithm>
#include <atomic>
#include <iterator>
#include <mutex>
#include <vector>
#include <sys/time.h>
#include <lvgl.h>

#include "board_config.h"
#include "hal/hal.h"
#include "lgfx_config.h"
#include "rtc.h"

// Partial render buffers: 40 lines of RGB565 each, double-buffered (internal RAM: DMA reads them).
// Not more: background jobs' thread stacks come from the same internal RAM (80 lines left no room).
// The SDK puts every malloc under 4 KB in internal RAM: strings, maps, JSON... they fragment it
// until a job's 16 KB thread stack has no block to fit in (50 KB free, 11 KB the largest). Only
// tiny ones stay there; the rest go to PSRAM. What must be internal (stacks, DMA, TLS, Wi-Fi) asks
// for it explicitly.
static constexpr size_t INTERNAL_MALLOC_MAX = 32;  // bytes
static constexpr int BUF_LINES = 40;
static constexpr uint32_t TOUCH_READ_MS = 10;  // LVGL's default is 30
// Screen off: touches read less often (still often enough to feel instant). Not by the touch line's
// interrupt (it never fired with the reads stopped), nor a slower CPU (Arduino's switch hangs the
// chip with Wi-Fi running: its build has no IDF power management to do it safely).
static constexpr uint32_t DOZE_TOUCH_READ_MS = 100;

// Buzzer: a square wave from the LEDC. Each note "plucks": its duty falls from 50% to a sliver, so
// it fades and thins like a struck string instead of a flat beep.
static constexpr int BUZZER_CHANNEL = 0;  // not 6/7: their timer drives the backlight
static constexpr uint32_t BUZZER_FULL = 512, BUZZER_TAIL = 40;  // duty at a note's start and end (of 1024)
static constexpr uint32_t ENVELOPE_STEP_MS = 5;
// The tune task: above the UI loop's priority, for steady timing.
static constexpr uint32_t TUNE_TASK_STACK = 2048;  // bytes
static constexpr UBaseType_t TUNE_TASK_PRIO = 2;

// Battery: VBAT through a 1/3 divider on PIN_BAT_ADC (Waveshare). Voltage to charge along a
// typical Li-ion discharge curve; read at most every BATTERY_READ_MS, averaged over a few samples.
static constexpr uint32_t BATTERY_READ_MS = 5000;
static constexpr int BATTERY_SAMPLES = 8;
static const struct {
  int mv, percent;
} BATTERY_CURVE[] = {{4200, 100}, {4100, 90}, {4000, 78}, {3900, 64}, {3800, 48}, {3750, 38},
                     {3700, 28},  {3650, 18}, {3600, 10}, {3500, 4},  {3300, 0}};
static constexpr uint32_t USB_FRAME_CHECK_MS = 5;  // a few USB frames apart

// Jobs (HTTP, TLS handshakes, PNG decoding) run on core 0 with Wi-Fi, below the UI loop's priority:
// on the UI's core they'd stop it mid-frame, leaving half-drawn widgets on screen.
static constexpr int JOB_CORE = 0, JOB_PRIO = 1;  // loopTask: core 1, priority 1

// Before this (Nov 2023), the clock hasn't been set: the chip boots at 1970.
static constexpr time_t CLOCK_SET_AFTER = 1700000000;

// On battery, the power stays on only while SYS_EN is high: set during static initialization, the
// earliest the firmware runs, so the power key needn't be held through the whole boot.
// gpio_config, not just a level: GPIO39-42 start out as JTAG pins. The level is then held through
// light sleep, when pins otherwise fall back to their sleep settings (and the power would drop).
static const bool power_held = [] {
  const gpio_num_t pin = (gpio_num_t)PIN_SYS_EN;
  gpio_config_t cfg = {};
  cfg.pin_bit_mask = 1ULL << PIN_SYS_EN;
  cfg.mode = GPIO_MODE_OUTPUT;
  gpio_set_level(pin, 1);  // already high the moment it becomes an output
  gpio_config(&cfg);
  gpio_set_level(pin, 1);
  gpio_hold_en(pin);
  return true;
}();

static LGFX tft;
static lv_indev_t *touch_indev = nullptr;
static std::atomic<bool> ntp_synced{false};
alignas(16) static uint8_t buf1[SCREEN_W * BUF_LINES * 2];  // aligned, or DMA falls back to a CPU copy
alignas(16) static uint8_t buf2[SCREEN_W * BUF_LINES * 2];

// By DMA: the strip goes out while LVGL draws the next one in the other buffer. LovyanGFX waits for
// a transfer to finish before starting the next, and LVGL alternates the two buffers, so a buffer is
// never drawn into while it's being sent. The bytes are swapped to the panel's order first, so
// LovyanGFX sends them as they are (no converting copy).
static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  const int32_t w = lv_area_get_width(area), h = lv_area_get_height(area);
  lv_draw_sw_rgb565_swap(px_map, (uint32_t)(w * h));
  if (tft.getStartCount() == 0) tft.startWrite();  // the bus stays the panel's: no setup per strip
  tft.pushImageDMA(area->x1, area->y1, w, h, (const lgfx::swap565_t *)px_map);
  lv_display_flush_ready(disp);
}

static void touch_read_cb(lv_indev_t *, lv_indev_data_t *data) {
  int32_t x, y;
  if (tft.getTouch(&x, &y)) {
    data->point.x = x;
    data->point.y = y;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

void hal_init() {
  heap_caps_malloc_extmem_enable(INTERNAL_MALLOC_MAX);
  Serial.begin(115200);
  hal_log("boot\n");

  tft.init();
  tft.setRotation(DISPLAY_ROTATION);

  // The RTC kept time while we were off: right from the first frame, before Wi-Fi and NTP.
  time_t utc;
  bool rtc_ok = rtc_read(utc);
  if (rtc_ok) {
    timeval tv = {utc, 0};
    settimeofday(&tv, nullptr);
  }
  hal_log("[clock] rtc: %s\n", rtc_ok ? "ok" : "not set");
  sntp_set_time_sync_notification_cb([](timeval *) { ntp_synced = true; });  // lwIP task: just flag it

  lv_tick_set_cb([]() -> uint32_t { return millis(); });

  lv_display_t *disp = lv_display_create(SCREEN_W, SCREEN_H);
  lv_display_set_flush_cb(disp, flush_cb);
  lv_display_set_buffers(disp, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_indev_t *touch = touch_indev = lv_indev_create();
  lv_indev_set_type(touch, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(touch, touch_read_cb);
  lv_timer_set_period(lv_indev_get_read_timer(touch), TOUCH_READ_MS);
}

static std::atomic<int> volume{100};  // percent
static std::mutex tune_mutex;
static std::vector<HalNote> tune_next;
static std::atomic<bool> tune_queued{false};
static TaskHandle_t tune_task = nullptr;

// Volume scales the duty: squared, so the slider's steps sound even (loudness isn't linear in it).
static void play_note(const HalNote &n) {
  const uint32_t v = (uint32_t)volume.load();
  const uint32_t full = BUZZER_FULL * v * v / 10000, tail = BUZZER_TAIL * v * v / 10000;
  const bool sounds = n.hz && full > 0;
  if (sounds) ledcWriteTone(BUZZER_CHANNEL, n.hz);
  for (uint32_t t = 0; t < n.ms && !tune_queued; t += ENVELOPE_STEP_MS) {
    if (sounds) {
      if (n.to_hz) ledcChangeFrequency(BUZZER_CHANNEL, n.hz + ((int32_t)n.to_hz - n.hz) * (int32_t)t / (int32_t)n.ms, 10);
      ledcWrite(BUZZER_CHANNEL, full - (full - tail) * t / n.ms);
    }
    vTaskDelay(pdMS_TO_TICKS(ENVELOPE_STEP_MS));
  }
  ledcWrite(BUZZER_CHANNEL, 0);
}

static void tune_loop(void *) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    std::vector<HalNote> notes;
    {
      std::lock_guard<std::mutex> lock(tune_mutex);
      notes.swap(tune_next);
      tune_queued = false;
    }
    for (const HalNote &n : notes) {
      if (tune_queued) break;  // a newer tune: it starts right away
      play_note(n);
    }
  }
}

void hal_play_tune(const HalNote *notes, size_t count) {
  if (!tune_task) {
    ledcSetup(BUZZER_CHANNEL, 1000, 10);
    ledcAttachPin(PIN_BUZZER, BUZZER_CHANNEL);
    ledcWrite(BUZZER_CHANNEL, 0);
    xTaskCreate(tune_loop, "tune", TUNE_TASK_STACK, nullptr, TUNE_TASK_PRIO, &tune_task);
  }
  {
    std::lock_guard<std::mutex> lock(tune_mutex);
    tune_next.assign(notes, notes + count);
    tune_queued = true;
  }
  xTaskNotifyGive(tune_task);
}

void hal_set_volume(int percent) { volume = std::clamp(percent, 0, 100); }

// A host sends a USB frame every millisecond: the frame counter moving means one is there.
bool hal_usb_connected() {
  static uint32_t last_frame = 0, read_at = 0;
  static bool connected = false;
  if (millis() - read_at < USB_FRAME_CHECK_MS) return connected;
  read_at = millis();
  uint32_t frame = REG_READ(USB_SERIAL_JTAG_FRAM_NUM_REG) & USB_SERIAL_JTAG_SOF_FRAME_INDEX;
  connected = frame != last_frame;
  last_frame = frame;
  return connected;
}

int hal_battery_percent() {
  static uint32_t read_at = 0;
  static int percent = -1;
  if (percent >= 0 && millis() - read_at < BATTERY_READ_MS) return percent;
  read_at = millis();
  uint32_t mv = 0;
  for (int i = 0; i < BATTERY_SAMPLES; i++) mv += analogReadMilliVolts(PIN_BAT_ADC);
  mv = mv * 3 / BATTERY_SAMPLES;
  percent = 0;
  if ((int)mv >= BATTERY_CURVE[0].mv) {
    percent = 100;
  } else {
    for (size_t i = 1; i < std::size(BATTERY_CURVE); i++) {
      if ((int)mv < BATTERY_CURVE[i].mv) continue;
      const auto &hi = BATTERY_CURVE[i - 1], &lo = BATTERY_CURVE[i];
      percent = lo.percent + (hi.percent - lo.percent) * ((int)mv - lo.mv) / (hi.mv - lo.mv);
      break;
    }
  }
  return percent;
}

void hal_set_brightness(int percent) { tft.setBrightness((uint8_t)(percent * 255 / 100)); }

void hal_display_sleep(bool sleep) {
  if (sleep) {
    tft.setBrightness(0);
    tft.sleep();  // panel off; the touch controller keeps reporting
  } else {
    tft.wakeup();
  }
}

void hal_doze(bool on) {
  lv_timer_set_period(lv_indev_get_read_timer(touch_indev), on ? DOZE_TOUCH_READ_MS : TOUCH_READ_MS);
}

// Waking by touch is unverified on the board: with the screen off but the chip awake, the touch
// line didn't change on a tap.
bool hal_light_sleep(uint32_t max_ms, HalWake &wake) {
  static const gpio_num_t WAKE_PINS[] = {(gpio_num_t)PIN_TP_INT, (gpio_num_t)PIN_BTN_CANCEL, (gpio_num_t)PIN_BTN_OK,
#ifdef PIN_BTN_NEXT
                                         (gpio_num_t)PIN_BTN_NEXT,
#endif
  };  // all active low
  esp_sleep_enable_timer_wakeup((uint64_t)max_ms * 1000);
  for (gpio_num_t pin : WAKE_PINS) gpio_wakeup_enable(pin, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  Serial.flush();
  esp_err_t err = esp_light_sleep_start();  // millis() keeps counting through it
  const bool by_pin = err == ESP_OK && esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO;
  wake = by_pin ? HalWake::Input : HalWake::Timer;
  for (gpio_num_t pin : WAKE_PINS) gpio_wakeup_disable(pin);
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  if (err != ESP_OK) {  // e.g. a wake pin was already low: the caller stops trying instead of looping
    hal_log("[sleep] rejected: %s\n", esp_err_to_name(err));
    return false;
  }
  return true;
}

uint32_t hal_millis() { return millis(); }
void hal_delay(uint32_t ms) { delay(ms); }
void hal_restart() {
  Serial.flush();
  esp_restart();
}
uint32_t hal_random() { return esp_random(); }
void hal_sha256(const void *data, size_t len, uint8_t out[32]) {
  mbedtls_sha256_ret((const unsigned char *)data, len, out, 0);
}

std::string hal_memory_report() {
  char buf[96];
  snprintf(buf, sizeof(buf), "internal RAM %u KB free, largest block %u KB",
           (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
           (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024));
  return buf;
}

void hal_set_thread_stack(uint32_t bytes) {
  esp_pthread_cfg_t cfg = esp_pthread_get_default_config();
  cfg.stack_size = bytes;
  cfg.pin_to_core = JOB_CORE;
  cfg.prio = JOB_PRIO;
  esp_pthread_set_cfg(&cfg);
}

// The clock is set from the RTC at boot, then by NTP once Wi-Fi is up.
bool hal_local_time(struct tm *out) { return getLocalTime(out, 0); }

void hal_update() {
  if (ntp_synced.exchange(false)) {  // on the main thread: the RTC shares the bus with touch reads
    rtc_write(time(nullptr));
    hal_log("[clock] ntp synced, rtc updated\n");
  }
}

void hal_set_timezone(const char *posix_tz) {
  setenv("TZ", posix_tz, 1);
  tzset();
}

bool hal_set_clock(int64_t epoch_ms) {
  timeval tv = {(time_t)(epoch_ms / 1000), (suseconds_t)(epoch_ms % 1000 * 1000)};
  settimeofday(&tv, nullptr);
  rtc_write(tv.tv_sec);
  return true;
}

int64_t hal_epoch_ms() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  if (tv.tv_sec < CLOCK_SET_AFTER) return 0;
  return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

uint16_t hal_device_id() { return (uint16_t)(ESP.getEfuseMac() >> 32); }  // last two MAC bytes

void hal_log(const char *fmt, ...) {
  char buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.print(buf);
}
