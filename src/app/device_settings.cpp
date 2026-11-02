#include "app/device_settings.h"

#include <stdlib.h>

#include <algorithm>
#include <string>

namespace key {
constexpr const char *BRIGHTNESS = "BRIGHTNESS";
constexpr const char *VOLUME = "VOLUME";
constexpr const char *SCREEN_OFF = "SCREEN_OFF";    // s
constexpr const char *SLEEP_AFTER = "SLEEP_AFTER";  // s
constexpr const char *TIMEZONE = "TIMEZONE";
constexpr const char *LIGHTS = "LIGHTS";              // 1 or 0
constexpr const char *LIGHTS_LEVEL = "LIGHTS_LEVEL";  // percent
}  // namespace key

DeviceSettings DeviceSettings::load(ConfigStore &store) {
  DeviceSettings s;
  std::string value;
  if (store.get(key::BRIGHTNESS, value)) s.brightness = std::clamp(atoi(value.c_str()), MIN_BRIGHTNESS, 100);
  if (store.get(key::VOLUME, value)) s.volume = std::clamp(atoi(value.c_str()), 0, 100);
  if (store.get(key::SCREEN_OFF, value) && atoi(value.c_str()) > 0) s.screenOffS = atoi(value.c_str());
  if (store.get(key::SLEEP_AFTER, value) && atoi(value.c_str()) > 0) s.sleepAfterS = atoi(value.c_str());
  if (store.get(key::TIMEZONE, value) && !value.empty()) s.timezone = value;
  if (store.get(key::LIGHTS, value)) s.lights = value != "0";
  if (store.get(key::LIGHTS_LEVEL, value)) s.lightsPercent = std::clamp(atoi(value.c_str()), 1, 100);
  return s;
}

void DeviceSettings::save(ConfigStore &store) const {
  store.set(key::BRIGHTNESS, std::to_string(brightness));
  store.set(key::VOLUME, std::to_string(volume));
  store.set(key::SCREEN_OFF, std::to_string(screenOffS));
  store.set(key::SLEEP_AFTER, std::to_string(sleepAfterS));
  store.set(key::TIMEZONE, timezone);
  store.set(key::LIGHTS, lights ? "1" : "0");
  store.set(key::LIGHTS_LEVEL, std::to_string(lightsPercent));
}
