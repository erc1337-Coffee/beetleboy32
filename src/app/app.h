#pragma once

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "account/catalog.h"
#include "account/keycloak.h"
#include "account/pokes.h"
#include "account/profile.h"
#include "app/device_config.h"
#include "app/device_settings.h"
#include "input/controller.h"
#include "media/lights.h"
#include "services/asset_cache.h"
#include "services/config_store.h"
#include "services/provisioning.h"
#include "services/wifi_station.h"
#include "ui/screen_manager.h"
#include "ui/theme.h"
#include "util/job.h"

// Owns the services and the screen stack, and drives the top-level flow:
//
//   boot ─(config saved)─> Connecting ─┬─(just set up)─> SigningIn ──────┬─(ok)─> LoadingProfile ─> Online
//    │                      ▲     │    └─(otherwise)──> CheckingAccount ─┘
//    │                      │     └─(fail)─> ConnectFailed (the known networks are retried underneath)
//    └─(nothing saved)─> Setup ─(submitted)
//
//   A refused account check goes back to Setup, for the account only. Any other failure of
//   SigningIn, CheckingAccount or LoadingProfile ends in CheckFailed (Retry / Set up again), after a
//   few automatic retries if it was a network error.
//   With a profile saved from an earlier boot, Connecting goes straight to Online: the account
//   check and a fresh load run behind Home (a refused login still ends in the login-expired setup).
//
// Screens are views: they call the public methods below; all transitions live here.
// The implementation is split by feature: app.cpp (startup, setup, profile, navigation, settings),
// app_session.cpp, app_beetle.cpp, app_icons.cpp, app_pokes.cpp and app_power.cpp (Wi-Fi, power).
class App {
 public:
  void begin();  // after lv_init() and hal_init()
  void update();

  // Every input event lands here first (from Controller, or e.g. an on-screen Back button).
  void handleInput(const InputEvent &event);

  // Setup and startup.
  void startSetup(SetupMode mode = SetupMode::Full);
  void setupByPhone();   // the device's own network and setup page
  void setupOnDevice();  // scan and keyboard
  void connect();
  void checkAccount();
  void loadProfile();
  void requestReset();
  void showRateLimited();  // the site said slow down: a 2 s popup
  void loginExpired();     // the SSO session is dead: popup, then setup for the account only

  // Navigation: screens opened from the menu, settings and demos.
  void openMenu();
  void openAbout();
  void openSettings();
  void openTimeSetter();
  void openWifi();
  void openSounds();
  void openLeds();
  void openSleepSettings();
  void openLightSettings();
  void openKeyboardDemo();
  void openDemos();  // the list (long-press on About's plaque)
  void openHunt();
  void openCheese();
  void openCraft();
  void closeTop();  // e.g. a picker, once picked
  void pushScreen(std::unique_ptr<Screen> screen);  // a screen's own sub-screen (a picker)
  bool gameLoaded() const;  // the game's data (inventory, cooldowns) has been read: Hunt and Craft need it
  bool perfOverlay() const { return perfOverlay_; }  // LVGL's FPS / CPU overlay
  void togglePerfOverlay();
  bool dozing() const { return power_ == Power::ScreenOff; }  // the main loop can go slower

  // Device settings (DeviceSettings), applied at once. `save` writes the value to the config store
  // (e.g. only when a slider is released).
  int brightness() const { return settings_.brightness; }  // backlight, percent
  void setBrightness(int percent, bool save);
  int volume() const { return settings_.volume; }
  void setVolume(int percent, bool save);
  // Idle seconds before the screen goes off, and before the chip sleeps (DeviceSettings).
  int screenOffSeconds() const { return settings_.screenOffS; }
  int sleepSeconds() const { return settings_.sleepAfterS; }
  void setScreenOffSeconds(int s);
  void setSleepSeconds(int s);
  // The LEDs: on or off, and the always-on green's brightness, in percent.
  bool lightsOn() const { return settings_.lights; }
  int lightsPercent() const { return settings_.lightsPercent; }
  void setLightsOn(bool on);
  void setLightsPercent(int percent);
  Lights &lights() { return lights_; }
  // The user says what time it is where they are: becomes their timezone (the clock stays UTC).
  void setLocalTime(int hour, int minute);

  // The account's theme and PFP: read with the profile, changed in Settings.
  const ThemeSetting &themeSetting() const;
  theme::UserPalette palette() const { return paletteFor(themeSetting()); }
  static theme::UserPalette paletteFor(const ThemeSetting &theme);
  enum class SaveState { Idle, Saving, Saved, Failed };
  void saveTheme(const ThemeSetting &theme);  // to the account; see themeSaveState()
  SaveState themeSaveState() const { return themeSave_; }
  // Settings > PFP: the PFPs the account owns, as a grid; picking one asks, then sets it (setPfp).
  void openPfpPicker();
  bool pfpsLoaded() const;  // the profile was loaded fresh: the PFP list and what updateProfile needs
  void setPfp(const std::string &project, const std::string &id);

  // Wi-Fi, for the Wi-Fi screens.
  struct WifiInfo {
    std::string ssid, ip;  // ip: "" without a link
    int rssi = 0;          // dBm, 0 without a link
    const char *status;    // "CONNECTED", "RECONNECTING", "OFFLINE"
  };
  WifiInfo wifiInfo() const;
  const std::vector<WifiCredentials> &knownWifi() const { return config_.networks; }
  bool forgetWifi(const std::string &ssid);  // false for the only one left: there'd be none to join
  void scanWifi();  // for the Add screen
  bool wifiScanning() const { return wifi_->scanning(); }
  std::vector<WifiStation::ScanResult> wifiScan() const { return wifi_->scanResults(); }
  // Joins it now (from the Add screen); it becomes a known network once it has worked. If it doesn't
  // work, the known networks take over again.
  void joinWifi(const WifiCredentials &network);
  void openKnownWifi();
  void openAddWifi();
  void pickWifi(const WifiStation::ScanResult &network);  // from the Add list: the keyboard if it needs a password, then joinWifi()

  // Beetle game (account/beetle.h). One task at a time; screens poll these.
  enum class BeetleTask { None, Refresh, Trap, Hunt, Cheese, Junk, Craft };
  const BeetleState &beetle() const;
  // The action running or queued, for screens: a background refresh isn't one (they show the
  // state they have meanwhile, and a press during it waits for it).
  BeetleTask beetleTask() const {
    return pendingTask_ != BeetleTask::None ? pendingTask_
           : beetleTask_ == BeetleTask::Refresh ? BeetleTask::None
                                                 : beetleTask_;
  }
  BeetleTask beetleFailed() const { return beetleFailed_; }    // the last one, if it failed
  // One request per press, gated on beetle(): call them only when it says the action is ready.
  void catchBeetle();  // the trap
  void huntBeetle();
  void claimCheese();
  void claimJunk();
  // Re-reads the counters and cooldowns, unless they were read under 3 s ago (any answer counts,
  // claims included) or a task is running: screens call it when shown, as often as they like.
  void refreshBeetle();
  // One craft (Craft screen); its outcome comes as a BeetleResult, with `craft` filled in.
  void craft(const CraftRequest &request);
  // The last finished action (not refreshes), for the result animation; the seq changes with each.
  struct BeetleResult {
    BeetleTask task = BeetleTask::None;
    bool ok = false;
    bool rateLimited = false;       // the rate-limit popup already said it
    std::vector<BeetleItem> items;  // what it won; empty if nothing (or it failed)
    CraftReply craft;               // Craft only
  };
  const BeetleResult &beetleResult() const { return beetleResult_; }
  uint32_t beetleResultSeq() const { return beetleResultSeq_; }
  // Every item's name, kind, rarity and icon (/api/beetle/cards): from flash, refreshed once per boot
  // when asked. Empty until either is there.
  const Catalog &catalog() const { return catalog_; }
  void loadCatalog();

  // Item icons, for the game's screens.
  static constexpr int ITEM_ICON_SIZE = 80;  // px; fetched once ever (kept in flash), then in RAM
  const lv_image_dsc_t *itemIcon(const std::string &icon_path);  // nullptr if it couldn't be fetched
  // The same, shrunk to fit size x size (made once, on first ask) for grids: drawn 1:1, no scaling.
  const lv_image_dsc_t *itemIcon(const std::string &icon_path, int size);
  // Gets these icons ready in the background (shipped, from flash, or downloaded), a few at a time:
  // itemIcon() returns each once it's there.
  // `first`: ahead of everything already waiting (e.g. the page on screen).
  void loadItemIcons(const std::vector<std::string> &icon_paths, bool first = false);
  void dropQueuedIcons(const std::vector<std::string> &icon_paths);  // not wanted anymore: not downloaded
  // Frees these icons and their shrunk copies (size 0), or only their copies of that size; still in
  // flash or the firmware, quick to load again. Only once nothing draws them (a picker, as it closes).
  void releaseItemIcons(const std::vector<std::string> &icon_paths, int size = 0);
  bool itemIconFailed(const std::string &icon_path) const { return iconsFailed_.count(icon_path) > 0; }  // not asked again

  // Pokes. The bell (menu) opens the notifications list: the pokes received lately, newest first,
  // one row per user. Its data is fetched one request at a time: the feed up to the rows on screen,
  // then those rows' PFPs, then their poke cooldowns. After POKE_AHEAD_IDLE_MS without a touch, the
  // same follows for the next page, then for the page before, so pages show already filled in.
  struct PokeEntry {
    Poke poke;
    bool pfpAsked = false;     // its PFP was requested (`pfp` stays empty if that failed)
    bool statusAsked = false;  // its poke cooldown was requested
    bool statusDone = false;   // that request finished; `status` holds the answer if statusKnown
    bool statusKnown = false;
    PokeStatus status;
    uint32_t statusAt = 0;      // hal_millis() when `status` was read
    OwnedImage pfp;             // empty until there (or if it couldn't be had)
  };
  static constexpr int POKE_ROW_PFP = 32;  // px
  void openNotifications();
  const std::deque<PokeEntry> &pokeList() const { return pokeList_; }  // a deque: entries never move (PFPs on screen)
  const lv_image_dsc_t *pokeListPfp(size_t i);  // nullptr until it's there
  bool pokeListMore() const { return pokeFeedMore_; }        // more to read past the end
  bool pokeListLoading() const { return pokeFeedLoading_; }  // reading more
  bool pokeListFailed() const { return pokeFeedFailed_; }    // the last read failed
  void showPokeRange(size_t first, size_t count);  // the rows on screen: read up to them, then their details
  void closePokeList();  // the screen closed: everything freed
  void pokeBack(const Poke &poke);
  bool pokingBack(const std::string &handle) const { return pokingBack_ && pokingBackHandle_ == handle; }

 private:
  // The site rate-limited us, or couldn't be reached: background requests pause this long.
  static constexpr uint32_t RATE_LIMITED_PAUSE_MS = 30 * 1000;

  // Job results, each filled on a job thread and taken on the main loop.
  struct LoginOutcome {
    AuthResult result = AuthResult::NetworkError;
    TokenSet tokens;
    AccountCookies cookies;  // from a password sign-in
    std::string detail;
  };
  struct ProfileOutcome {
    AuthResult result = AuthResult::NetworkError;
    std::unique_ptr<Profile> profile;
    std::string detail;
  };
  struct AvatarOutcome {
    bool ok = false;
    OwnedImage avatar;
    std::string detail;
  };
  struct PfpOutcome {
    AuthResult result = AuthResult::NetworkError;
    std::string detail;
  };
  struct ThemeOutcome {
    AuthResult result = AuthResult::NetworkError;
    ThemeSetting theme;
    std::string detail;
  };
  struct BeetleOutcome {
    AuthResult result = AuthResult::NetworkError;
    BeetleState state;
    std::vector<BeetleStep> steps;
    std::vector<std::pair<std::string, OwnedImage>> icons;  // newly fetched, by icon path
    CraftReply craft;
    std::string detail;
  };
  struct CatalogOutcome {
    AuthResult result = AuthResult::NetworkError;
    Catalog catalog;
    std::string detail;
  };
  struct IconsOutcome {
    std::vector<std::pair<std::string, OwnedImage>> icons;  // by icon path
    std::vector<std::string> remote;  // not downloaded yet: rate-limited or unreachable
    std::vector<std::string> failed;  // the site has none (a 404...): not asked again
    int downloads = 0;
    bool rateLimited = false, unreachable = false;  // the rest of the batch is in `remote`
  };
  struct PokeOutcome {
    AuthResult result = AuthResult::NetworkError;
    std::vector<Poke> fresh;  // newer than pokeSeenAt_, newest first
    int64_t newest = -1;      // the newest poke's time, fresh or not (-1: none at all)
    PokeStatus status;        // fresh[0]'s sender's
    bool statusKnown = false;
    OwnedImage pfp;           // fresh[0]'s sender's; empty if it couldn't be had
    std::string detail;
    std::string readFailure;  // why fresh[0] couldn't be marked read ("" if it was)
  };
  struct PokeFeedOutcome {
    AuthResult result = AuthResult::NetworkError;
    std::vector<Poke> pokes;
    bool more = false;
    uint32_t gen = 0;
    std::string detail;
  };
  struct PokeDetailOutcome {  // one row's PFP, or its cooldown
    uint32_t gen = 0;
    size_t index = 0;
    std::string handle;
    bool isPfp = false;
    OwnedImage pfp;
    bool statusKnown = false;
    PokeStatus status;
    std::string displayName, pfpUrl;  // with the cooldown, from the profile
  };
  struct PokeBackOutcome {
    AuthResult result = AuthResult::NetworkError;
    std::string name, detail;
  };

  // Startup: the mode machine (app.cpp).
  enum class Mode { Setup, Connecting, ConnectFailed, SigningIn, CheckingAccount, LoadingProfile, CheckFailed, Online };
  // Where setup is. Choose: picking between the phone and the device. With a phone: Starting (the
  // setup network is coming up), Join (waiting for the phone to join it), Portal (a phone has joined
  // and can open the setup page). OnDevice: the on-screen scan and keyboard.
  enum class SetupStep { Choose, Starting, Join, Portal, OnDevice };
  void updateSetup();
  void showSetupChoice();
  void setupWifiPicked(const WifiCredentials &network);
  void askSetupEmail();  // then the password
  void finishDeviceSetup();
  void applySetup(SetupSubmission &submitted);
  void updateConnecting();
  void updateConnectFailed();
  void wifiJoined();  // carries on with startup once a network is joined
  void signIn();      // the one-time email/password sign-in, after setup
  void updateSigningIn();
  void forgetSignIn();
  void updateCheckingAccount();
  void updateLoadingProfile();
  bool autoRetry(void (App::*step)(), const std::string &detail);
  void showCheckFailed(const char *title, const std::string &detail, void (App::*retry)());
  void showBusy(const std::string &text);
  void factoryReset();
  void showPicture();  // the hidden full-screen picture (touch held TOUCH_HOLD_MS); a tap hides it

  // Online: the profile on Home, and the jobs' results polled each frame (app.cpp).
  bool startFromSavedProfile();  // false if nothing was saved
  void enterOnline(std::unique_ptr<Profile> profile);
  void updateOnline();
  void reloadProfile();  // in the background, once Home is up
  static ProfileOutcome fetchProfile(HttpClient &http, AssetCache &cache, const std::string &token);
  void applyProfile(Profile &fresh);
  void startAvatarJob();  // if the avatar isn't cached for the current URL
  void setAvatar(OwnedImage avatar);
  void redrawHome();
  void applySettings();  // brightness and timezone, to the hardware
  void pollProfile();    // the avatar and a reloaded profile
  void pollSaves();      // the PFP and theme saves

  // Session: the access token (app_session.cpp).
  void setSession(const TokenSet &tokens);
  // Calls `then(true)` once session_ holds an access token that won't expire in the next minute,
  // refreshing it first if needed; `then(false)` if that failed.
  void withFreshToken(std::function<void(bool ok)> then);
  // Starts `work(http, token)` on `job` once withFreshToken() has a token; `no_token()` otherwise.
  template <typename Outcome, typename Work, typename NoToken>
  void startAuthed(Job<Outcome> &job, Work work, NoToken no_token);
  bool pollTokenRefresh();  // false if the login expired: setup has taken over

  // Beetle game (app_beetle.cpp).
  bool beetleTaskAllowed(BeetleTask task) const;
  void startBeetleTask(BeetleTask task);
  void pollBeetle();       // a task's result; the first read retried until it works
  void pollCatalog();
  void runPendingTask();   // a press that waited for a refresh
  void checkCooldowns();   // a sound, and a confirming read, as each cooldown runs out

  // Item icons (app_icons.cpp).
  void loadLocalIcons();  // the queue's ones on the device, a few each frame
  void startIconsJob();   // downloads
  void pollIcons();

  // Pokes (app_pokes.cpp).
  void startPokeCheck();
  void showPoke(const Poke &poke, const PokeStatus &status, bool statusKnown, OwnedImage pfp);
  void pollPokes();     // the new pokes check, and the next one scheduled
  void pollPokeBack();
  void updatePokeList();
  void startPokeFeed();
  bool startPokeDetail(size_t from, size_t to);

  // Wi-Fi and power (app_power.cpp).
  std::vector<WifiCredentials> wifiCandidates() const;
  void rememberJoinedWifi();  // the network joined goes first in the known ones
  void scheduleWifiRetry();
  void updateLink();   // Wi-Fi supervision while online
  void reconnectWifi(const char *message);  // nullptr: quietly (only a failure says something)
  void updatePower();  // screen off, light sleep
  void screenOff();
  void wakeScreen();
  void sleepUntilNextCooldown();

  // Services. Declared before the jobs: cache_ outlives every job that uses it.
  Controller controller_;
  ScreenManager screens_;
  std::unique_ptr<ConfigStore> store_;
  std::unique_ptr<AssetCache> cache_;
  std::unique_ptr<Provisioning> provisioning_;
  std::unique_ptr<WifiStation> wifi_;
  DeviceConfig config_;
  DeviceSettings settings_;
  Lights lights_;
  lv_obj_t *picture_ = nullptr;  // on LVGL's top layer, over every screen
  bool perfOverlay_ = false;

  // Startup.
  Mode mode_ = Mode::Setup;
  Job<LoginOutcome> loginJob_;
  // From setup, in RAM only, until the sign-in succeeds or setup restarts (kept for Retry).
  std::string signInEmail_, signInPassword_;
  bool freshWifi_ = false;  // config_.wifi was just entered in setup and hasn't been joined yet
  SetupSubmission deviceSetup_;  // collected screen by screen in the on-device setup (RAM only)
  SetupNetwork setupNetwork_;
  SetupStep setupStep_ = SetupStep::Starting;
  uint32_t setupStartedAt_ = 0;  // the phone way's network went up
  SetupMode setupMode_ = SetupMode::Full;
  int autoRetries_ = 0;                // of the current startup step
  void (App::*retryStep_)() = nullptr;  // what the retry timer runs
  Mode retryMode_ = Mode::Setup;       // ...if still in this mode

  // Session.
  TokenSet session_;  // access token lives in RAM only
  uint32_t sessionAt_ = 0;  // hal_millis() when session_ was issued
  Job<LoginOutcome> refreshJob_;
  std::vector<std::function<void(bool)>> tokenWaiters_;  // non-empty while refreshJob_ runs

  // Profile, theme and PFP.
  std::unique_ptr<Profile> profile_;  // owned here: the home screen draws from it (avatar pixels)
  OwnedImage retiredAvatar_;  // the one replaced last: a Home on its way out may still draw it
  Job<ProfileOutcome> profileJob_;
  Job<AvatarOutcome> avatarJob_;
  bool avatarFetching_ = false;
  Job<ThemeOutcome> themeJob_;
  SaveState themeSave_ = SaveState::Idle;
  Job<PfpOutcome> pfpJob_;
  bool pfpSaving_ = false;

  // Beetle game.
  Job<BeetleOutcome> beetleJob_;
  BeetleTask beetleTask_ = BeetleTask::None;
  BeetleTask pendingTask_ = BeetleTask::None;  // pressed during a refresh: runs after it, if still allowed
  BeetleTask beetleFailed_ = BeetleTask::None;
  BeetleResult beetleResult_;
  uint32_t beetleResultSeq_ = 0;
  CraftRequest craftRequest_;  // the one BeetleTask::Craft sends
  // Readiness last frame. Going from waiting to ready (by our clock) asks the server once to
  // confirm: no polling, one request per cooldown that runs out. True at start: nothing to confirm.
  bool cheeseWasReady_ = true, beetlesWasReady_ = true;
  bool trapWasReady_ = true, huntWasReady_ = true;  // for the sound: each rings on its own
  bool beetleWasLoaded_ = false;
  uint32_t beetleRetryAt_ = 0;  // the first read failed: tried again from then on
  Job<CatalogOutcome> catalogJob_;
  Catalog catalog_;
  bool catalogAsked_ = false;  // fetched (or being fetched) this boot

  // Item icons.
  Job<IconsOutcome> iconsJob_;
  std::map<std::string, OwnedImage> itemIcons_;  // by icon path
  std::map<std::pair<std::string, int>, OwnedImage> smallIcons_;  // by icon path and size; never replaced
  std::vector<std::string> iconQueue_;      // asked for, not looked for on the device yet (loadLocalIcons)
  std::vector<std::string> iconDownloads_;  // not on the device: downloaded a few at a time, paced
  std::vector<std::string> iconBatch_;      // the ones iconsJob_ is loading
  std::set<std::string> iconsFailed_;   // couldn't be loaded (e.g. a 404): not asked again this boot
  std::set<std::string> iconsUnwanted_; // in iconBatch_ but dropped since: its result is let go
  bool iconsLoading_ = false;
  uint32_t iconsResumeAt_ = 0;  // downloads pause until then (paced: the site rate-limits)
  int iconsBurst_ = 0;          // downloads since the last pause

  // Pokes: the notifications are read every so often while online; one newer than the newest seen
  // shows, and is marked read on the site. That time is kept in the store, so a poke shows once,
  // even across restarts.
  Job<PokeOutcome> pokeJob_;
  int64_t pokeSeenAt_ = -1;  // -1: none seen yet (the first read only takes note)
  bool pokeSeenLoaded_ = false;
  bool pokeChecking_ = false;
  uint32_t pokeCheckAt_ = 0;
  OwnedImage pokePfp_, retiredPokePfp_;  // the poke on screen's sender's; the one before (may still be drawn)
  Job<PokeBackOutcome> pokeBackJob_;
  bool pokingBack_ = false;
  std::string pokingBackHandle_;
  // The notifications list (see PokeEntry).
  Job<PokeFeedOutcome> pokeFeedJob_;
  Job<PokeDetailOutcome> pokeDetailJob_;
  std::deque<PokeEntry> pokeList_;
  int pokeFeedPage_ = 0;  // feed pages read
  bool pokeFeedMore_ = false, pokeFeedLoading_ = false, pokeFeedFailed_ = false, pokeDetailLoading_ = false;
  size_t pokeRangeFirst_ = 0, pokeRangeCount_ = 0;  // the rows on screen (0: the list isn't)
  uint32_t pokeListGen_ = 0;  // +1 on each open: a job's result from a list since closed is let go

  // Wi-Fi while online. Up: connected. Reconnecting: a connect is under way, after the link dropped,
  // joinWifi(), a timed retry or waking from sleep (the last two are quiet: only a failure is
  // announced). Down: no network, tried again at wifiRetryAt_ (Settings > Wi-Fi can add one meanwhile).
  // The same timer retries under the startup "No Wi-Fi" screen; wifiRetrying_ is set while a try runs.
  enum class Link { Up, Reconnecting, Down };
  Link link_ = Link::Up;
  bool quietReconnect_ = false;
  bool wifiOutage_ = false;  // the user was told it's gone: "Wi-Fi back" when it returns
  uint32_t wifiRetryAt_ = 0;
  bool wifiRetrying_ = false;
  bool joiningWifi_ = false;  // joinWifi() under way: its outcome gets its own toast

  // Power. Awake, or ScreenOff after settings_.screenOffS without input: Wi-Fi stays up, touches
  // are read less often and pokes checked less often. After settings_.sleepAfterS the chip
  // light-sleeps with Wi-Fi off until the next cooldown ends or a touch or button wakes it.
  enum class Power { Awake, ScreenOff };
  Power power_ = Power::Awake;
  bool lightSleepWorks_ = true;  // false once the platform said no (the sim)
};

template <typename Outcome, typename Work, typename NoToken>
void App::startAuthed(Job<Outcome> &job, Work work, NoToken no_token) {
  withFreshToken([this, &job, work, no_token](bool ok) {
    if (!ok) {
      no_token();
      return;
    }
    job.start([work, token = session_.accessToken] {
      auto http = create_http_client();
      return work(*http, token);
    });
  });
}
