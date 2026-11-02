#pragma once

#include <string>

#include "services/config_store.h"

// User preferences kept on the device (not the account). Saved in the same store as the setup
// details, so a factory reset clears them too.
struct DeviceSettings {
  static constexpr int MIN_BRIGHTNESS = 10;  // percent; never fully dark, or nothing could be seen to undo it
  int brightness = 80;                       // percent
  int volume = 60;                           // buzzer, percent (0: silent)
  // Idle seconds before the screen turns off, and before the chip light-sleeps with Wi-Fi off until
  // the next cooldown (App::Power).
  int screenOffS = 60;
  int sleepAfterS = 30 * 60;
  // The LEDs: on or off, and the always-on green's brightness (percent; the effects are at full).
  bool lights = true;
  int lightsPercent = 25;

  // POSIX TZ for local time (backgrounds, the clock). Default: Irvine, California (US Pacific, with
  // daylight saving). Setting the time by hand replaces it with that fixed offset from UTC.
  static constexpr const char *DEFAULT_TIMEZONE = "PST8PDT,M3.2.0,M11.1.0";
  std::string timezone = DEFAULT_TIMEZONE;

  static DeviceSettings load(ConfigStore &store);
  void save(ConfigStore &store) const;
};
