// Wi-Fi (the known networks, and keeping the link up while online) and power (screen off, sleep).
#include <lvgl.h>

#include "app/app.h"
#include "hal/hal.h"
#include "ui/toast.h"

// No Wi-Fi (at startup or once online): the known networks are tried again this long after the last try.
static constexpr uint32_t WIFI_RETRY_MS = 3000;
static constexpr uint32_t WIFI_TOAST_MS = 3000;
static constexpr uint32_t WIFI_BACK_TOAST_MS = 1500;
static constexpr uint32_t WIFI_PENDING_TOAST_MS = 60 * 1000;  // "Joining..." and the like: replaced when it ends
// Light sleep with no cooldown to wait for still wakes up after this, to check in.
static constexpr uint32_t MAX_SLEEP_MS = 6 * 60 * 60 * 1000;

// --- Known networks ---------------------------------------------------------------------------

App::WifiInfo App::wifiInfo() const {
  const char *status = link_ == Link::Up ? "CONNECTED" : link_ == Link::Reconnecting ? "RECONNECTING" : "OFFLINE";
  std::string ssid = wifi_->ssid();
  return {ssid.empty() ? config_.wifi.ssid : ssid, wifi_->ip(), wifi_->rssi(), status};
}

bool App::forgetWifi(const std::string &ssid) {
  if (config_.networks.size() <= 1) return false;
  for (auto it = config_.networks.begin(); it != config_.networks.end(); ++it) {
    if (it->ssid != ssid) continue;
    config_.networks.erase(it);
    if (config_.wifi.ssid == ssid) config_.wifi = config_.networks[0];  // stays joined until it drops
    config_.save(*store_);
    hal_log("[app] forgot \"%s\"\n", ssid.c_str());
    break;
  }
  return true;
}

void App::scanWifi() { wifi_->startScan(); }

void App::joinWifi(const WifiCredentials &network) {
  hal_log("[app] joining \"%s\"\n", network.ssid.c_str());
  config_.wifi = network;
  freshWifi_ = true;
  joiningWifi_ = true;
  link_ = Link::Reconnecting;
  quietReconnect_ = false;
  wifi_->connect({network});
  show_toast(LV_SYMBOL_WIFI, ("Joining " + network.ssid + "...").c_str(), WIFI_PENDING_TOAST_MS);
}

// Every known network, the one just set up first.
std::vector<WifiCredentials> App::wifiCandidates() const {
  std::vector<WifiCredentials> networks;
  if (freshWifi_ || config_.networks.empty()) networks.push_back(config_.wifi);
  for (const WifiCredentials &n : config_.networks)
    if (networks.empty() || n.ssid != networks[0].ssid) networks.push_back(n);
  return networks;
}

void App::rememberJoinedWifi() {
  std::string ssid = wifi_->ssid();
  hal_log("[app] wi-fi: joined \"%s\"\n", ssid.c_str());
  WifiCredentials joined = config_.wifi;
  if (!(freshWifi_ && joined.ssid == ssid)) {
    for (const WifiCredentials &n : config_.networks)
      if (n.ssid == ssid) joined = n;
  }
  freshWifi_ = false;
  auto same = [](const WifiCredentials &a, const WifiCredentials &b) { return a.ssid == b.ssid && a.password == b.password; };
  if (!config_.networks.empty() && same(config_.networks[0], joined) && same(config_.wifi, joined)) return;
  config_.rememberWifi(joined);
  config_.save(*store_);  // only when it changed: saving stalls the screen a moment
}

void App::scheduleWifiRetry() { wifiRetryAt_ = hal_millis() + WIFI_RETRY_MS; }

// --- The link while online --------------------------------------------------------------------

void App::updateLink() {
  switch (link_) {
    case Link::Up:
      if (!wifi_->linkUp()) {
        hal_log("[app] wi-fi lost\n");
        reconnectWifi("Reconnecting...");
      }
      break;
    case Link::Reconnecting:
      if (wifi_->state() == WifiStation::State::Connected) {
        hal_log("[app] wi-fi back\n");
        rememberJoinedWifi();  // maybe another known network than before
        link_ = Link::Up;
        if (joiningWifi_) {
          show_toast(LV_SYMBOL_WIFI, ("Joined " + wifi_->ssid()).c_str(), WIFI_TOAST_MS);
        } else if (wifiOutage_) {
          show_toast(LV_SYMBOL_WIFI, "Wi-Fi back", WIFI_BACK_TOAST_MS);  // replaces "reconnecting"
        }
        joiningWifi_ = wifiOutage_ = false;
        refreshBeetle();
      } else if (wifi_->state() == WifiStation::State::Failed && joiningWifi_) {
        // The new network didn't work: back to the known ones, quietly unless they fail too.
        hal_log("[app] joining failed: %s\n", wifi_->failureReason());
        show_toast(LV_SYMBOL_WIFI, ("Can't join " + config_.wifi.ssid + ": " + wifi_->failureReason()).c_str(),
                   WIFI_TOAST_MS);
        joiningWifi_ = freshWifi_ = false;
        config_.wifi = config_.networks.empty() ? config_.wifi : config_.networks[0];
        reconnectWifi(nullptr);
      } else if (wifi_->state() == WifiStation::State::Failed) {
        hal_log("[app] wi-fi reconnect failed: %s\n", wifi_->failureReason());
        link_ = Link::Down;
        scheduleWifiRetry();
        // Said once per outage: over "Reconnecting..." when the link dropped, else on the first quiet
        // try that fails. Later tries are silent.
        if (!wifiOutage_ || !quietReconnect_) show_toast(LV_SYMBOL_WIFI, "No Wi-Fi", WIFI_TOAST_MS);
        wifiOutage_ = true;
      }
      break;
    case Link::Down:
      if ((int32_t)(hal_millis() - wifiRetryAt_) >= 0) reconnectWifi(nullptr);
      break;
  }
}

void App::reconnectWifi(const char *message) {
  link_ = Link::Reconnecting;
  quietReconnect_ = !message;
  wifi_->connect(wifiCandidates());
  if (message) {
    wifiOutage_ = true;
    show_toast(LV_SYMBOL_WIFI, message, WIFI_PENDING_TOAST_MS);
  }
}

// --- Power ------------------------------------------------------------------------------------

void App::updatePower() {
  uint32_t idle = lv_display_get_inactive_time(nullptr);  // touches, and buttons (Controller)
  if (power_ == Power::Awake) {
    if (idle >= (uint32_t)settings_.screenOffS * 1000) screenOff();
    return;
  }
  if (controller_.darkPress() || idle < (uint32_t)settings_.screenOffS * 1000) {  // touched while dark
    hal_log("[app] screen on: touch/button\n");
    wakeScreen();
  } else if (idle >= (uint32_t)settings_.sleepAfterS * 1000 && lightSleepWorks_ && link_ == Link::Up && beetleTask_ == BeetleTask::None) {
    sleepUntilNextCooldown();
  }
}

void App::screenOff() {
  hal_log("[app] screen off\n");
  power_ = Power::ScreenOff;
  hal_display_sleep(true);
  // Nothing is drawn while dark (Home's scrolling bio would keep it busy forever): one last frame
  // (the sim's black veil), then the refresh pauses. Animations keep running.
  lv_display_t *display = lv_display_get_default();
  lv_refr_now(display);
  lv_timer_pause(lv_display_get_refr_timer(display));
  controller_.setDark(true);
  hal_doze(true);
}

void App::wakeScreen() {
  hal_log("[app] screen on\n");
  power_ = Power::Awake;
  hal_doze(false);
  controller_.setDark(false);
  controller_.swallowInput();  // what woke it doesn't also press what's under it
  lv_display_t *display = lv_display_get_default();
  lv_timer_resume(lv_display_get_refr_timer(display));
  lv_obj_invalidate(lv_screen_active());  // all of it: it changed in the dark
  lv_obj_invalidate(lv_display_get_layer_top(display));
  hal_display_sleep(false);
  hal_set_brightness(settings_.brightness);
  lv_display_trigger_activity(nullptr);  // restart the idle timer
}

void App::sleepUntilNextCooldown() {
  const BeetleState &b = profile_->beetle;
  const uint32_t now = hal_millis();
  uint32_t until = MAX_SLEEP_MS;
  for (uint32_t left : {b.cheeseMsLeft(now), b.beetlesMsLeft(now, hal_epoch_ms())})
    if (left > 0 && left < until) until = left;

  // Wi-Fi can't stay connected through light sleep: off now, back (quietly) on waking.
  wifi_->disconnect();
  hal_log("[app] light sleep, up to %u s\n", (unsigned)(until / 1000));
  HalWake wake = HalWake::Timer;
  bool slept = hal_light_sleep(until, wake);
  reconnectWifi(nullptr);
  if (!slept) {  // not on this platform (the sim): the screen stays off, Wi-Fi up, and we stop trying
    hal_log("[app] light sleep unsupported here\n");
    lightSleepWorks_ = false;
    return;
  }
  hal_log("[app] woke up (%s)\n", wake == HalWake::Input ? "touch/button" : "timer");
  if (wake == HalWake::Input) wakeScreen();
  // Timer: the cooldown check wakes the screen once it's ready; a mere check-in sleeps again.
}
