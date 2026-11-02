#pragma once

#include <lvgl.h>

// Time-of-day backgrounds. Phase boundaries are defined in background.cpp.
enum class DayPhase { Morning, Day, Night };

DayPhase day_phase_for_hour(int hour);  // hour: 0-23, local time
const lv_image_dsc_t *background_for(DayPhase phase);

// Applies the background for the current local time. Cheap to call every loop: it reads the
// clock at most once a minute and only restyles when the phase changes.
void background_update();
