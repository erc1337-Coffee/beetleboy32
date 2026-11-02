#include "ui/background.h"

#include "assets/assets.h"
#include "hal/hal.h"
#include "ui/theme.h"

static constexpr int MORNING_FROM = 5;  // [05:00, 10:00) morning
static constexpr int DAY_FROM = 10;     // [10:00, 18:00) day
static constexpr int NIGHT_FROM = 18;   // [18:00, 05:00) night
static constexpr DayPhase UNKNOWN_TIME = DayPhase::Day;  // before the clock has been set
static constexpr uint32_t CHECK_EVERY_MS = 60 * 1000;

DayPhase day_phase_for_hour(int hour) {
  if (hour >= MORNING_FROM && hour < DAY_FROM) return DayPhase::Morning;
  if (hour >= DAY_FROM && hour < NIGHT_FROM) return DayPhase::Day;
  return DayPhase::Night;
}

const lv_image_dsc_t *background_for(DayPhase phase) {
  switch (phase) {
    case DayPhase::Morning: return &img_morning;
    case DayPhase::Day: return &img_day;
    case DayPhase::Night: return &img_night;
  }
  return &img_day;
}

void background_update() {
  static bool applied = false;
  static DayPhase current;
  static uint32_t checked_at;

  uint32_t now = hal_millis();
  if (applied && now - checked_at < CHECK_EVERY_MS) return;
  checked_at = now;

  struct tm local;
  DayPhase phase = hal_local_time(&local) ? day_phase_for_hour(local.tm_hour) : UNKNOWN_TIME;
  if (applied && phase == current) return;
  applied = true;
  current = phase;
  theme::set_background(background_for(phase));
  hal_log("[ui] background %s\n",
          phase == DayPhase::Morning ? "morning" : phase == DayPhase::Day ? "day" : "night");
}
