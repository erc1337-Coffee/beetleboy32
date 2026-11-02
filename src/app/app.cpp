#include "app/app.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <iterator>

#include <src/display/lv_display_private.h>  // the perf overlay's label

#include "app/menu_config.h"
#include "assets/assets.h"
#include "hal/hal.h"
#include "media/sounds.h"
#include "ui/background.h"
#include "ui/screens/about_screen.h"
#include "ui/screens/add_wifi_screen.h"
#include "ui/screens/busy_screen.h"
#include "ui/screens/cheese_screen.h"
#include "ui/screens/craft_screen.h"
#include "ui/screens/demo_screen.h"
#include "ui/screens/home_screen.h"
#include "ui/screens/hunt_screen.h"
#include "ui/screens/item_picker_screen.h"
#include "ui/screens/keyboard_screen.h"
#include "ui/screens/known_wifi_screen.h"
#include "ui/screens/leds_screen.h"
#include "ui/screens/light_settings_screen.h"
#include "ui/screens/menu_screen.h"
#include "ui/screens/message_screen.h"
#include "ui/screens/settings_screen.h"
#include "ui/screens/setup_choice_screen.h"
#include "ui/screens/setup_join_screen.h"
#include "ui/screens/setup_portal_screen.h"
#include "ui/screens/sleep_screen.h"
#include "ui/screens/sounds_screen.h"
#include "ui/screens/time_screen.h"
#include "ui/screens/wifi_screen.h"
#include "ui/toast.h"
#include "util/secret.h"

static constexpr int SETUP_PASSWORD_CHARS = 10;  // the setup network's
// The phone way's open setup network (it takes the account's password) goes down after this.
static constexpr uint32_t SETUP_PHONE_TIMEOUT_MS = 10 * 60 * 1000;
static constexpr uint32_t SETUP_TIMEOUT_TOAST_MS = 4000;
// Startup steps failing for a network reason (or a rate limit) try again by themselves, waiting
// longer each time, before the error screen asks the user.
static constexpr int AUTO_RETRIES = 3;
static constexpr uint32_t RETRY_DELAY_MS = 2000;  // doubled after each try
// The PFP picker: 3 a row, as on the website, 6 a page.
static constexpr int32_t PFP_TILE = 72;  // px
static constexpr int PFP_PAGE_SIZE = 6;
static constexpr int PFP_IN_USE = 4, PFP_OTHER = 0;  // tile colors (ui_rarity_colors)

static SetupNetwork make_setup_network() {
  // No 0/O/1/l/I: the password is also shown as text and may be typed by hand.
  static const char alphabet[] = "abcdefghjkmnpqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  SetupNetwork n;
  char ssid[24];
  snprintf(ssid, sizeof(ssid), "BeetleBoy32-%04X", hal_device_id());
  n.ssid = ssid;
  for (int i = 0; i < SETUP_PASSWORD_CHARS; i++) n.password += alphabet[hal_random() % (std::size(alphabet) - 1)];
  return n;
}

void App::begin() {
  theme::init();
  background_update();
  controller_.begin([this](const InputEvent &e) { handleInput(e); });
  screens_.begin(controller_.keypad());
  store_ = create_config_store();
  cache_ = create_asset_cache();
  provisioning_ = create_provisioning();
  wifi_ = create_wifi_station();

  lv_sysmon_hide_performance(lv_display_get_default());  // LVGL shows it from the start: off until asked
  settings_ = DeviceSettings::load(*store_);
  applySettings();
  config_ = DeviceConfig::load(*store_);
  if (config_.valid()) {
    connect();
  } else {
    startSetup();
  }
}

void App::update() {
  controller_.update();

  background_update();

  switch (mode_) {
    case Mode::Setup: updateSetup(); break;
    case Mode::Connecting: updateConnecting(); break;
    case Mode::SigningIn: updateSigningIn(); break;
    case Mode::CheckingAccount: updateCheckingAccount(); break;
    case Mode::LoadingProfile: updateLoadingProfile(); break;
    case Mode::ConnectFailed: updateConnectFailed(); break;
    case Mode::CheckFailed: break;
    case Mode::Online: updateOnline(); break;
  }

  screens_.update();

  lights_.setGlow(settings_.lights && power_ == Power::Awake, settings_.lightsPercent);  // dark with the screen
  // A hunt, claim or craft on its way: the chase (a refresh isn't one).
  lights_.setBusy((beetleTask_ != BeetleTask::None && beetleTask_ != BeetleTask::Refresh) ||
                  pendingTask_ != BeetleTask::None);
  lights_.update();
}

void App::handleInput(const InputEvent &event) {
  if (screens_.dispatch(event)) return;
  switch (event.action) {
    case InputAction::Back:
      if (mode_ == Mode::Setup && setupStep_ != SetupStep::Choose && setupStep_ != SetupStep::OnDevice) {
        showSetupChoice();  // from the phone way: pick again
      } else {
        screens_.pop();
      }
      break;
    case InputAction::Reset: requestReset(); break;
    case InputAction::Hold: showPicture(); break;
    default: break;
  }
}

// --- Setup ------------------------------------------------------------------------------------

void App::startSetup(SetupMode mode) {
  // Redo one part only when the other part is there to keep.
  if (mode == SetupMode::AccountOnly && !config_.wifi.valid()) mode = SetupMode::Full;
  if (mode == SetupMode::WifiOnly && !config_.account.valid()) mode = SetupMode::Full;
  static const char *const MODES[] = {"full", "wi-fi only", "account only"};
  static_assert(std::size(MODES) == (size_t)SetupMode::AccountOnly + 1, "one name per SetupMode, in order");
  hal_log("[app] setup (%s)\n", MODES[(int)mode]);
  forgetSignIn();
  freshWifi_ = false;
  hide_toast();  // e.g. a Wi-Fi toast still showing
  if (power_ != Power::Awake) wakeScreen();  // e.g. the login expired while it slept
  wifi_->disconnect();
  setupMode_ = mode;
  mode_ = Mode::Setup;
  showSetupChoice();
}

void App::showSetupChoice() {
  provisioning_->stop();  // back from the phone way (or setup restarted from within setup)
  wipe_secret(deviceSetup_.accountPassword);
  deviceSetup_ = SetupSubmission{};
  setupStep_ = SetupStep::Choose;
  screens_.reset(std::make_unique<SetupChoiceScreen>(*this, setupMode_));
}

void App::setupByPhone() {
  hal_log("[app] setup with the phone\n");
  setupNetwork_ = make_setup_network();
  provisioning_->start(setupNetwork_, setupMode_, config_.wifi.ssid);
  setupStep_ = SetupStep::Starting;
  setupStartedAt_ = hal_millis();
  showBusy("Starting setup...");
}

// Screens pushed over the choice, one per thing to enter: Back goes to the previous one.
void App::setupOnDevice() {
  hal_log("[app] setup on the device\n");
  setupStep_ = SetupStep::OnDevice;
  if (setupMode_ == SetupMode::AccountOnly) {
    askSetupEmail();
  } else {
    screens_.push(std::make_unique<AddWifiScreen>(*this));  // then pickWifi()
  }
}

void App::setupWifiPicked(const WifiCredentials &network) {
  deviceSetup_.wifi = network;
  if (setupMode_ == SetupMode::WifiOnly) {
    finishDeviceSetup();
  } else {
    askSetupEmail();
  }
}

void App::askSetupEmail() {
  screens_.push(std::make_unique<KeyboardScreen>(*this, "Email", [this](const std::string &email) {
    deviceSetup_.email = email;
    screens_.push(std::make_unique<KeyboardScreen>(*this, "Password", [this](const std::string &password) {
      deviceSetup_.accountPassword = password;
      finishDeviceSetup();
    }, /*masked=*/true));
  }));
}

void App::finishDeviceSetup() {
  SetupSubmission submitted = std::move(deviceSetup_);
  deviceSetup_ = SetupSubmission{};
  applySetup(submitted);
}

void App::updateSetup() {
  wifi_->update();  // the on-device way scans
  if (setupStep_ == SetupStep::Choose || setupStep_ == SetupStep::OnDevice) return;
  if (hal_millis() - setupStartedAt_ >= SETUP_PHONE_TIMEOUT_MS) {
    hal_log("[app] setup with the phone timed out\n");
    showSetupChoice();  // takes the network down
    show_toast(LV_SYMBOL_WIFI, "Setup timed out", SETUP_TIMEOUT_TOAST_MS);
    return;
  }
  provisioning_->update();

  SetupStep want = !provisioning_->ready()          ? SetupStep::Starting
                   : provisioning_->clientConnected() ? SetupStep::Portal
                                                      : SetupStep::Join;
  if (want != setupStep_ && want != SetupStep::Starting) {
    setupStep_ = want;
    if (want == SetupStep::Join) {
      screens_.reset(std::make_unique<SetupJoinScreen>(*this, setupNetwork_));
    } else {
      screens_.reset(std::make_unique<SetupPortalScreen>(*this, provisioning_->portalUrl()));
    }
  }

  SetupSubmission submitted;
  if (provisioning_->takeSubmission(submitted)) {
    provisioning_->stop();
    applySetup(submitted);
  }
}

// What setup collected, either way.
void App::applySetup(SetupSubmission &submitted) {
  if (setupMode_ != SetupMode::AccountOnly) {
    config_.wifi = submitted.wifi;  // joins the known networks once it has worked
    freshWifi_ = true;
  }
  if (setupMode_ != SetupMode::WifiOnly) {
    // The saved cookies stay until the sign-in replaces them: a setup that fails loses nothing.
    signInEmail_ = submitted.email;
    signInPassword_ = submitted.accountPassword;
  }
  wipe_secret(submitted.accountPassword);
  wipe_secret(submitted.email);
  hal_log("[app] setup received for \"%s\"\n", config_.wifi.ssid.c_str());
  config_.save(*store_);
  connect();  // then the sign-in (password given) or the usual check with the kept cookies
}

// --- Connecting and signing in ----------------------------------------------------------------

void App::connect() {
  mode_ = Mode::Connecting;
  wifiRetrying_ = false;
  // Just set up: only that network, so a failure is about it. Otherwise any known one.
  std::vector<WifiCredentials> networks = freshWifi_ ? std::vector<WifiCredentials>{config_.wifi} : wifiCandidates();
  hal_log("[app] connecting (%u known networks)\n", (unsigned)networks.size());
  wifi_->connect(networks);
  showBusy(networks.size() == 1 ? "Connecting to " + networks[0].ssid + "..." : "Connecting to Wi-Fi...");
}

void App::updateConnecting() {
  wifi_->update();
  switch (wifi_->state()) {
    case WifiStation::State::Connected:
      wifiJoined();
      break;
    case WifiStation::State::Failed: {
      hal_log("[app] connect failed: %s\n", wifi_->failureReason());
      mode_ = Mode::ConnectFailed;
      scheduleWifiRetry();
      std::string reason = wifi_->failureReason();
      if (!reason.empty()) reason[0] = (char)toupper((unsigned char)reason[0]);
      // Right after setup it's about the network just entered; otherwise none of the known ones worked.
      std::string body = freshWifi_ ? "\"" + config_.wifi.ssid + "\": " + wifi_->failureReason() : reason;
      screens_.reset(std::make_unique<MessageScreen>(
          *this, "connect_failed", freshWifi_ ? "Can't connect" : "No Wi-Fi", body,
          std::vector<MessageScreen::Action>{
              {freshWifi_ ? "Edit" : "Add", [this] { startSetup(SetupMode::WifiOnly); }},  // keeps the account
              {"Retry", [this] { connect(); }},
          },
          nullptr, MessageScreen::Look::Blue));
      break;
    }
    default:
      break;
  }
}

// The error screen stays while the known networks are tried again now and then underneath (no busy
// screen); joining one carries on as if the first try had worked.
void App::updateConnectFailed() {
  wifi_->update();
  switch (wifi_->state()) {
    case WifiStation::State::Connected:
      wifiJoined();
      break;
    case WifiStation::State::Failed:
      if (wifiRetrying_) {
        wifiRetrying_ = false;
        hal_log("[app] wi-fi retry failed: %s\n", wifi_->failureReason());
        scheduleWifiRetry();
      } else if ((int32_t)(hal_millis() - wifiRetryAt_) >= 0) {
        wifiRetrying_ = true;
        wifi_->connect(wifiCandidates());
      }
      break;
    default:
      break;
  }
}

void App::wifiJoined() {
  wifiRetrying_ = false;
  rememberJoinedWifi();
  if (!signInPassword_.empty()) {
    signIn();  // just set up: sign in with the password
  } else if (!startFromSavedProfile()) {
    checkAccount();
  }
}

void App::signIn() {
  hal_log("[app] signing in\n");  // never the email or password
  mode_ = Mode::SigningIn;
  // Copied straight into the job (no locals left behind); it wipes its copy when done.
  loginJob_.start([email = signInEmail_, password = signInPassword_]() mutable {
    LoginOutcome o;
    auto http = create_http_client();
    o.result = keycloak_password_login(*http, email, password, o.cookies, o.tokens, o.detail);
    wipe_secret(password);
    return o;
  });
  showBusy("Signing in...");
}

bool App::autoRetry(void (App::*step)(), const std::string &detail) {
  if (autoRetries_ >= AUTO_RETRIES) {
    autoRetries_ = 0;  // a Retry press gets the same patience
    return false;
  }
  uint32_t delay = RETRY_DELAY_MS << autoRetries_++;
  hal_log("[app] retry %d/%d in %u s: %s\n", autoRetries_, AUTO_RETRIES, (unsigned)(delay / 1000), detail.c_str());
  retryStep_ = step;
  retryMode_ = mode_;
  lv_timer_t *timer = lv_timer_create([](lv_timer_t *t) {
    App *app = static_cast<App *>(lv_timer_get_user_data(t));
    if (app->mode_ == app->retryMode_) (app->*app->retryStep_)();  // not if setup was started meanwhile
  }, delay, this);
  lv_timer_set_repeat_count(timer, 1);
  return true;
}

void App::updateSigningIn() {
  wifi_->update();
  LoginOutcome o;
  if (!loginJob_.take(o)) return;
  hal_log("[app] sign-in: %s\n", o.detail.c_str());

  switch (o.result) {
    case AuthResult::Ok:
      autoRetries_ = 0;
      forgetSignIn();
      setSession(o.tokens);
      config_.account = o.cookies;
      config_.save(*store_);
      loadProfile();
      break;
    case AuthResult::Rejected:  // wrong password, or a step only the website can do
      forgetSignIn();
      mode_ = Mode::CheckFailed;
      screens_.reset(std::make_unique<MessageScreen>(
          *this, "sign_in_failed", "Can't sign in", o.detail,
          std::vector<MessageScreen::Action>{{"Set up again", [this] { startSetup(); }}}, nullptr,
          MessageScreen::Look::Blue));
      break;
    case AuthResult::NetworkError:
    case AuthResult::RateLimited:
      if (autoRetry(&App::signIn, o.detail)) break;
      showCheckFailed("Can't reach remilia.net", o.detail, &App::signIn);  // Retry keeps the password
      break;
  }
}

void App::forgetSignIn() {
  wipe_secret(signInPassword_);
  wipe_secret(signInEmail_);
}

void App::checkAccount() {
  hal_log("[app] checking account\n");
  mode_ = Mode::CheckingAccount;
  AccountCookies cookies = config_.account;
  std::string refresh_token = DeviceConfig::loadRefreshToken(*store_);
  loginJob_.start([cookies, refresh_token] {
    LoginOutcome o;
    auto http = create_http_client();
    // The refresh token (offline_access) outlives the website session behind the cookies: it goes
    // first, the cookies only if it's refused.
    o.result = refresh_token.empty() ? AuthResult::Rejected
                                     : keycloak_refresh(*http, refresh_token, o.tokens, o.detail);
    if (o.result == AuthResult::Rejected) o.result = keycloak_login(*http, cookies, o.tokens, o.detail);
    return o;
  });
  showBusy("Checking account...");
}

void App::updateCheckingAccount() {
  wifi_->update();
  LoginOutcome o;
  if (!loginJob_.take(o)) return;
  hal_log("[app] account check: %s\n", o.detail.c_str());

  switch (o.result) {
    case AuthResult::Ok:
      autoRetries_ = 0;
      setSession(o.tokens);
      loadProfile();
      break;
    case AuthResult::Rejected:
      loginExpired();
      break;
    case AuthResult::NetworkError:
    case AuthResult::RateLimited:
      if (autoRetry(&App::checkAccount, o.detail)) break;
      showCheckFailed("Can't reach remilia.net", o.detail, &App::checkAccount);
      break;
  }
}

App::ProfileOutcome App::fetchProfile(HttpClient &http, AssetCache &cache, const std::string &token) {
  ProfileOutcome o;
  o.profile = std::make_unique<Profile>();
  o.result = fetch_profile(http, cache, token, *o.profile, o.detail);
  return o;
}

// Right after a sign-in or account check: the token is fresh, no withFreshToken() needed.
void App::loadProfile() {
  hal_log("[app] loading profile\n");
  mode_ = Mode::LoadingProfile;
  profileJob_.start([token = session_.accessToken, cache = cache_.get()] {
    auto http = create_http_client();
    return fetchProfile(*http, *cache, token);
  });
  showBusy("Loading profile...");
}

void App::updateLoadingProfile() {
  wifi_->update();
  ProfileOutcome o;
  if (!profileJob_.take(o)) return;
  hal_log("[app] profile: %s\n", o.detail.c_str());

  if (o.result == AuthResult::Ok) {
    autoRetries_ = 0;
    save_profile(*store_, *o.profile);
    enterOnline(std::move(o.profile));
  } else {
    // Not auto-retrying a refused token: if the API's auth changed, that would loop forever.
    if (o.result != AuthResult::Rejected && autoRetry(&App::loadProfile, o.detail)) return;
    showCheckFailed("Loading profile failed", "", &App::loadProfile);  // the cause is in the log
  }
}

void App::showCheckFailed(const char *title, const std::string &detail, void (App::*retry)()) {
  mode_ = Mode::CheckFailed;
  screens_.reset(std::make_unique<MessageScreen>(
      *this, "check_failed", title, detail,
      std::vector<MessageScreen::Action>{
          {"Retry", [this, retry] { (this->*retry)(); }},
          {"Set up again", [this] { startSetup(); }},
      }, nullptr, MessageScreen::Look::Blue));
}

// Loading steps follow each other without fading (screens they lead to, like Home, fade in).
void App::showBusy(const std::string &text) {
  screens_.reset(std::make_unique<BusyScreen>(*this, text), Transition::Cut);
}

void App::showRateLimited() { show_toast(&img_menu_pfp_084, "Rate limited!"); }

void App::loginExpired() {
  hal_log("[app] login expired\n");
  startSetup(SetupMode::AccountOnly);
  show_toast(&img_menu_pfp_031, "Login expired!");  // after startSetup: it clears popups
}

void App::requestReset() {
  static constexpr const char *RESET_SCREEN = "reset_confirm";
  Screen *top = screens_.top();
  if (top && strcmp(top->name(), RESET_SCREEN) == 0) return;

  auto dismiss = [this] { screens_.pop(); };
  screens_.push(std::make_unique<MessageScreen>(
      *this, RESET_SCREEN, "Reset BeetleBoy32?", "Wi-Fi and account details will be erased.",
      std::vector<MessageScreen::Action>{
          {"Keep", dismiss},  // first = focused, so a blind OK press is harmless
          {"Erase", [this] { factoryReset(); }},
      },
      dismiss));
}

// Everything saved goes (the account's tokens and cookies, Wi-Fi, settings); cached images stay.
// Then a restart, so nothing of the old account is left in memory either (sessions, jobs underway).
void App::factoryReset() {
  hal_log("[app] factory reset\n");
  store_->clear();
  hal_restart();
}

void App::showPicture() {
  if (!picture_) {
    picture_ = lv_image_create(lv_layer_top());
    lv_image_set_src(picture_, &img_dynasty);
    lv_obj_add_flag(picture_, LV_OBJ_FLAG_CLICKABLE);  // also keeps touches from the screen below
    lv_obj_add_event_cb(picture_, [](lv_event_t *e) {
      lv_obj_add_flag(static_cast<lv_obj_t *>(lv_event_get_current_target(e)), LV_OBJ_FLAG_HIDDEN);
    }, LV_EVENT_CLICKED, nullptr);
  }
  lv_obj_remove_flag(picture_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(picture_);
  play_picture_tune();
}

// --- Online -----------------------------------------------------------------------------------

bool App::startFromSavedProfile() {
  auto saved = std::make_unique<Profile>();
  if (!load_saved_profile(*store_, *cache_, *saved)) return false;
  hal_log("[app] home from the saved profile\n");
  session_ = TokenSet{};  // expired: the first request trades the refresh token (withFreshToken)
  session_.refreshToken = DeviceConfig::loadRefreshToken(*store_);
  enterOnline(std::move(saved));
  reloadProfile();
  return true;
}

void App::enterOnline(std::unique_ptr<Profile> profile) {
  profile_ = std::move(profile);
  mode_ = Mode::Online;
  link_ = Link::Up;
  beetleWasLoaded_ = false;
  screens_.reset(std::make_unique<HomeScreen>(*this, *profile_));
  // The rest of the card arrives while it's on screen.
  startBeetleTask(BeetleTask::Refresh);
  startAvatarJob();
}

// Every job's result is taken here, in a fixed order, then whatever is due next is started.
void App::updateOnline() {
  wifi_->update();
  updateLink();
  updatePower();
  if (!pollTokenRefresh()) return;
  pollBeetle();
  pollCatalog();
  pollIcons();
  pollPokes();
  updatePokeList();
  pollPokeBack();
  pollProfile();
  runPendingTask();
  checkCooldowns();
  pollSaves();
}

void App::reloadProfile() {
  AssetCache *cache = cache_.get();
  startAuthed(profileJob_, [cache](HttpClient &http, const std::string &token) { return fetchProfile(http, *cache, token); },
              [] {});  // no token: the saved card stays, and the beetle read retries the token
}

// A fresh load over the saved card: Home redraws only if something it shows changed.
void App::applyProfile(Profile &fresh) {
  Profile &p = *profile_;
  bool redraw = fresh.displayName != p.displayName || fresh.bio != p.bio || fresh.theme != p.theme;
  p.handle = fresh.handle;
  p.displayName = fresh.displayName;
  p.bio = fresh.bio;
  p.theme = fresh.theme;
  p.cover = fresh.cover;
  if (fresh.editable) {
    p.editable = true;
    p.pfpProject = fresh.pfpProject;
    p.pfpId = fresh.pfpId;
    p.location = fresh.location;
    p.achievements = std::move(fresh.achievements);
    p.friends = std::move(fresh.friends);
    p.pfps = std::move(fresh.pfps);
  }
  if (fresh.pfpUrl != p.pfpUrl) {
    p.pfpUrl = fresh.pfpUrl;
    setAvatar(std::move(fresh.avatar));  // the new one if cached, else empty until downloaded
  }
  save_profile(*store_, p);
  if (redraw) redrawHome();
  startAvatarJob();
}

void App::startAvatarJob() {
  if (avatarFetching_ || !profile_->avatar.empty() || profile_->pfpUrl.empty()) return;
  avatarFetching_ = true;
  AssetCache *cache = cache_.get();
  startAuthed(avatarJob_, [url = profile_->pfpUrl, cache](HttpClient &http, const std::string &token) {
    AvatarOutcome o;
    o.ok = fetch_avatar(http, *cache, token, url, o.avatar, o.detail);
    return o;
  }, [this] { avatarFetching_ = false; });
}

void App::setAvatar(OwnedImage avatar) {
  bool was_shown = !profile_->avatar.empty();
  retiredAvatar_ = std::move(profile_->avatar);
  profile_->avatar = std::move(avatar);
  if (was_shown) redrawHome();  // else Home adds it when it sees it
}

void App::redrawHome() {
  Screen *top = screens_.top();
  if (top && strcmp(top->name(), "home") == 0) {
    screens_.replace(std::make_unique<HomeScreen>(*this, *profile_));
  } else {
    screens_.rebuildHidden();
  }
}

void App::pollProfile() {
  AvatarOutcome avatar;
  if (avatarJob_.take(avatar)) {
    hal_log("[app] avatar: %s\n", avatar.detail.c_str());
    avatarFetching_ = false;
    if (avatar.ok) setAvatar(std::move(avatar.avatar));
  }

  ProfileOutcome reloaded;
  if (profileJob_.take(reloaded)) {
    hal_log("[app] profile reload: %s\n", reloaded.detail.c_str());
    if (reloaded.result == AuthResult::Ok) applyProfile(*reloaded.profile);  // else the saved card stays
  }
}

// --- Theme and PFP ----------------------------------------------------------------------------

const ThemeSetting &App::themeSetting() const {
  static const ThemeSetting BEFORE_PROFILE{true, 230};  // lavender, until the profile has loaded
  return profile_ ? profile_->theme : BEFORE_PROFILE;
}

theme::UserPalette App::paletteFor(const ThemeSetting &theme) {
  return theme.colored ? theme::user_palette(theme.hue) : theme::default_palette();
}

void App::saveTheme(const ThemeSetting &theme) {
  if (!profile_ || themeSave_ == SaveState::Saving) return;
  hal_log("[app] saving theme (%s, hue %d)\n", theme.colored ? "color" : "default", theme.hue);
  themeSave_ = SaveState::Saving;
  startAuthed(themeJob_, [theme, cover = profile_->cover](HttpClient &http, const std::string &token) {
    ThemeOutcome o;
    o.theme = theme;
    o.result = update_theme(http, token, theme, cover, o.detail);
    return o;
  }, [this] {
    themeSave_ = SaveState::Failed;
    show_toast(nullptr, "Theme not saved");
  });
}

bool App::pfpsLoaded() const { return profile_ && profile_->editable; }

// The one in use highlighted. Picking asks first.
void App::openPfpPicker() {
  if (!pfpsLoaded()) return;
  std::vector<ItemPickerScreen::Item> items;
  for (const PfpChoice &p : profile_->pfps) {
    ItemPickerScreen::Item it;
    it.key = p.project + "#" + p.id;
    it.icon = p.url;
    it.name = p.project;
    it.label = "#" + p.id;
    it.rarity = p.project == profile_->pfpProject && p.id == profile_->pfpId ? PFP_IN_USE : PFP_OTHER;
    items.push_back(std::move(it));
  }
  ItemPickerScreen::Options options;
  options.tile = PFP_TILE;
  options.centered = true;
  options.pageSize = PFP_PAGE_SIZE;
  options.releaseAll = true;  // no one else shows them
  screens_.push(std::make_unique<ItemPickerScreen>(*this, std::move(items), options, [this](const std::string &key) {
    size_t hash = key.rfind('#');
    std::string project = key.substr(0, hash), id = key.substr(hash + 1);
    show_toast(nullptr, ("Set " + project + " #" + id + " as PFP?").c_str(), 0,
               {{"Cancel", nullptr}, {"Set", [this, project, id] { setPfp(project, id); }}});
  }));
}

void App::setPfp(const std::string &project, const std::string &id) {
  if (!pfpsLoaded() || pfpSaving_) return;
  hal_log("[app] setting PFP %s #%s\n", project.c_str(), id.c_str());
  pfpSaving_ = true;
  auto snapshot = std::make_shared<Profile>();  // what update_pfp sends back unchanged
  snapshot->editable = true;
  snapshot->handle = profile_->handle;
  snapshot->displayName = profile_->displayName;
  snapshot->bio = profile_->bio;
  snapshot->location = profile_->location;
  snapshot->achievements = profile_->achievements;
  snapshot->friends = profile_->friends;
  startAuthed(pfpJob_, [project, id, snapshot](HttpClient &http, const std::string &token) {
    PfpOutcome o;
    o.result = update_pfp(http, token, *snapshot, project, id, o.detail);
    return o;
  }, [this] {
    pfpSaving_ = false;
    show_toast(nullptr, "PFP not saved");
  });
}

void App::pollSaves() {
  PfpOutcome pfp;
  if (pfpJob_.take(pfp)) {
    hal_log("[app] PFP save: %s\n", pfp.detail.c_str());
    pfpSaving_ = false;
    if (pfp.result == AuthResult::Ok) {
      show_toast(nullptr, "PFP updated");
      reloadProfile();  // the new picture comes with it
    } else if (pfp.result == AuthResult::RateLimited) {
      showRateLimited();
    } else {
      show_toast(nullptr, "PFP not saved");
    }
  }

  ThemeOutcome saved;
  if (themeJob_.take(saved)) {
    hal_log("[app] theme save: %s\n", saved.detail.c_str());
    if (saved.result == AuthResult::Ok) {
      profile_->theme = saved.theme;
      save_profile(*store_, *profile_);
      screens_.rebuildHidden();  // they were drawn with the old colors
      themeSave_ = SaveState::Saved;
    } else {
      themeSave_ = SaveState::Failed;
      if (saved.result == AuthResult::RateLimited) {
        showRateLimited();
      } else {
        show_toast(nullptr, "Theme not saved");  // saved on leaving Settings: the user has moved on
      }
    }
  }
}

// --- Navigation -------------------------------------------------------------------------------

void App::openMenu() {
  std::vector<MenuScreen::Item> items;
  for (const MenuEntry &entry : HOME_MENU) {
    auto open = entry.open;
    auto ready = entry.ready;
    std::function<bool()> enabled;
    if (ready) enabled = [this, ready] { return (this->*ready)(); };
    items.push_back({entry.label, entry.icon,
                     [this, open] {
                       if (open) (this->*open)();
                     },
                     enabled});
  }
  screens_.push(std::make_unique<MenuScreen>(*this, std::move(items), [this] { openNotifications(); }));
}

void App::openAbout() { screens_.push(std::make_unique<AboutScreen>(*this)); }
void App::openTimeSetter() { screens_.push(std::make_unique<TimeScreen>(*this)); }
void App::openWifi() { screens_.push(std::make_unique<WifiScreen>(*this)); }
void App::openSounds() { screens_.push(std::make_unique<SoundsScreen>(*this)); }
void App::openLeds() { screens_.push(std::make_unique<LedsScreen>(*this)); }
void App::openLightSettings() { screens_.push(std::make_unique<LightSettingsScreen>(*this)); }
void App::openSleepSettings() { screens_.push(std::make_unique<SleepScreen>(*this)); }
void App::openDemos() { screens_.push(std::make_unique<DemoScreen>(*this)); }
void App::openKnownWifi() { screens_.push(std::make_unique<KnownWifiScreen>(*this)); }
void App::openAddWifi() { screens_.push(std::make_unique<AddWifiScreen>(*this)); }
void App::openCheese() { screens_.push(std::make_unique<CheeseScreen>(*this)); }

void App::openSettings() {
  if (themeSave_ != SaveState::Saving) themeSave_ = SaveState::Idle;
  screens_.push(std::make_unique<SettingsScreen>(*this));
}

bool App::gameLoaded() const { return profile_ && profile_->beetle.loaded; }

void App::openHunt() {
  if (gameLoaded()) screens_.push(std::make_unique<HuntScreen>(*this));
}

void App::openCraft() {
  if (gameLoaded()) screens_.push(std::make_unique<CraftScreen>(*this));
}

void App::closeTop() { screens_.pop(); }
void App::pushScreen(std::unique_ptr<Screen> screen) { screens_.push(std::move(screen)); }

void App::openKeyboardDemo() {
  screens_.push(std::make_unique<KeyboardScreen>(*this, "Type anything", [this](const std::string &) {
    screens_.pop();
  }));
}

// LVGL makes the overlay with the display; showing it again would make a second one (and the two
// would share, and zero, the same frame counts): its label is just hidden or shown.
void App::togglePerfOverlay() {
  perfOverlay_ = !perfOverlay_;
  lv_obj_t *label = lv_display_get_default()->perf_label;
  if (label) lv_obj_update_flag(label, LV_OBJ_FLAG_HIDDEN, !perfOverlay_);
}

// Back on the Wi-Fi screen afterwards, which shows how it goes.
void App::pickWifi(const WifiStation::ScanResult &network) {
  if (mode_ == Mode::Setup) {  // setup on the device: noted, then the account
    if (!network.secured) {
      setupWifiPicked({network.ssid, ""});
      return;
    }
    std::string ssid = network.ssid;
    screens_.push(std::make_unique<KeyboardScreen>(*this, ssid, [this, ssid](const std::string &password) {
      setupWifiPicked({ssid, password});
    }));
    return;
  }
  if (!network.secured) {
    screens_.pop();  // the Add list
    joinWifi({network.ssid, ""});
    return;
  }
  std::string ssid = network.ssid;
  screens_.push(std::make_unique<KeyboardScreen>(*this, ssid, [this, ssid](const std::string &password) {
    screens_.pop();  // the keyboard
    screens_.pop();  // the Add list
    joinWifi({ssid, password});
  }));
}

// --- Device settings --------------------------------------------------------------------------

void App::applySettings() {
  hal_set_brightness(settings_.brightness);
  hal_set_volume(settings_.volume);
  hal_set_timezone(settings_.timezone.c_str());
}

void App::setBrightness(int percent, bool save) {
  settings_.brightness = std::clamp(percent, DeviceSettings::MIN_BRIGHTNESS, 100);
  hal_set_brightness(settings_.brightness);
  if (save) settings_.save(*store_);
}

void App::setVolume(int percent, bool save) {
  settings_.volume = std::clamp(percent, 0, 100);
  hal_set_volume(settings_.volume);
  if (save) settings_.save(*store_);
}

void App::setScreenOffSeconds(int s) {
  settings_.screenOffS = s;
  settings_.save(*store_);
}

void App::setSleepSeconds(int s) {
  settings_.sleepAfterS = s;
  settings_.save(*store_);
}

void App::setLightsOn(bool on) {
  settings_.lights = on;
  settings_.save(*store_);
}

void App::setLightsPercent(int percent) {
  settings_.lightsPercent = percent;
  settings_.save(*store_);
}

// Today's date as far as the firmware knows: the day it was built.
static struct tm build_date() {
  static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  char month[4] = {};
  int day = 1, year = 2026;
  sscanf(__DATE__, "%3s %d %d", month, &day, &year);
  struct tm t = {};
  const char *m = strstr(months, month);
  t.tm_mon = m ? (int)(m - months) / 3 : 0;
  t.tm_mday = day;
  t.tm_year = year - 1900;
  return t;
}

void App::setLocalTime(int hour, int minute) {
  int64_t now_ms = hal_epoch_ms();
  if (now_ms > 0) {
    // The clock knows UTC (NTP, RTC): what the user enters is their offset from it.
    time_t now = (time_t)(now_ms / 1000);
    struct tm utc;
    gmtime_r(&now, &utc);
    int offset = hour * 60 + minute - (utc.tm_hour * 60 + utc.tm_min);  // minutes east of UTC
    if (offset < -12 * 60) offset += 24 * 60;
    if (offset > 14 * 60) offset -= 24 * 60;
    offset = (offset >= 0 ? offset + 7 : offset - 7) / 15 * 15;  // zones are whole quarter hours
    char tz[16];
    // POSIX counts west of UTC: UTC+2 is "LOC-2:00".
    snprintf(tz, sizeof(tz), "LOC%c%d:%02d", offset > 0 ? '-' : '+', abs(offset) / 60, abs(offset) % 60);
    settings_.timezone = tz;
    hal_log("[app] time set: %02d:%02d, timezone %s\n", hour, minute, tz);
  } else {
    // No clock yet: keep the zone and set the clock from the entered time, on the build day.
    struct tm local = build_date();
    local.tm_hour = hour;
    local.tm_min = minute;
    local.tm_isdst = -1;
    hal_set_clock((int64_t)mktime(&local) * 1000);
    hal_log("[app] time set: %02d:%02d (no clock: date assumed)\n", hour, minute);
  }
  applySettings();
  settings_.save(*store_);
  background_update();  // the day phase may have changed
}
