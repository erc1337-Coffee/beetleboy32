#pragma once

#include "account/profile.h"
#include "ui/screen.h"

// Theme picker: a hue slider like the website's and a white square for the light theme (the
// website's "default"), sent to the account on leaving. Below, device-only: screen brightness, buzzer
// volume, then PFP (the PFP picker), zzz (SleepScreen), LED (LightSettingsScreen), Time (TimeScreen)
// and Wi-Fi (WifiScreen).

class SettingsScreen : public Screen {
 public:
  static constexpr int TILE_COUNT = 5;  // the bottom row

  explicit SettingsScreen(App &app) : Screen(app) {}
  const char *name() const override { return "settings"; }
  void update() override;  // the clock, and the tiles waiting for data
  bool onInput(const InputEvent &event) override;  // Back saves the theme, if changed

 protected:
  void build(lv_obj_t *root) override;

 private:
  void showChoice();
  void showBrightness();
  void showVolume();
  static void onBrightness(lv_event_t *e);
  static void onVolume(lv_event_t *e);
  static void onHue(lv_event_t *e);
  static void onLight(lv_event_t *e);
  static void onTile(lv_event_t *e);

  ThemeSetting choice_;
  lv_obj_t *slider_ = nullptr;
  lv_obj_t *light_ = nullptr;
  lv_obj_t *brightness_ = nullptr;
  lv_obj_t *brightnessValue_ = nullptr;
  lv_obj_t *volume_ = nullptr;
  lv_obj_t *volumeValue_ = nullptr;
  lv_obj_t *clock_ = nullptr;  // top right
  lv_obj_t *tiles_[TILE_COUNT] = {};
  int timeShown_ = -1;  // minute of the day on screen
};
