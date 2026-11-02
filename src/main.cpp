#include <lvgl.h>

#include <algorithm>

#include "app/app.h"
#include "hal/hal.h"

static App app;

static void app_setup() {
  lv_init();
  hal_init();
  app.begin();
}

// Sleeps until LVGL's next timer, at most LOOP_MAX_WAIT_MS, so jobs and Wi-Fi are polled between frames.
static constexpr uint32_t LOOP_MAX_WAIT_MS = 10;
// Screen off (App::dozing): nothing drawn, touches read every 100 ms. LVGL's timers just run late.
static constexpr uint32_t LOOP_DOZE_WAIT_MS = 50;

static void app_loop() {
  hal_update();
  app.update();
  uint32_t wait = lv_timer_handler();
  hal_delay(app.dozing() ? LOOP_DOZE_WAIT_MS : std::clamp(wait, (uint32_t)1, LOOP_MAX_WAIT_MS));
}

#ifdef ARDUINO
#include <Arduino.h>

// The loop runs LVGL (drawing included) and every screen: Arduino's 8 KB default is tight.
SET_LOOP_TASK_STACK_SIZE(16 * 1024);
void setup() { app_setup(); }
void loop() { app_loop(); }
#else
int main() {
  app_setup();
  for (;;) app_loop();
}
#endif
